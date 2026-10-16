/*
 * firmware_oop.ino
 *
 * Abstract base class FirmwareBase defines the timing measurement loop.
 * BaselineFirmware and HardenedFirmware override only the security logic.
 * Toggle HARDENED_MODE to switch between the two.
 */

#include <LiquidCrystal.h>
#include <avr/io.h>
#include <EEPROM.h>
#include <string.h>

// ── 컴파일 시 모드 선택 ───────────────────────────
// #define HARDENED_MODE    // 주석 해제 → 하드닝 버전
                            // 주석 유지 → 베이스라인 버전

// ── 하드웨어 핀 ───────────────────────────────────
#define SERVO_PIN  9
#define BUZZER_PIN 8
LiquidCrystal lcd(12, 11, 5, 4, 3, 13);

// ── EEPROM ────────────────────────────────────────
#define EEPROM_ADDR 0

// ── Timer1 ────────────────────────────────────────
void timer1_init() {
  TCCR1A = 0;
  TCCR1B = (1 << CS10);
  TCNT1  = 0;
}

inline float ticks_to_us(uint16_t t0, uint16_t t1) {
  return (uint16_t)(t1 - t0) * 0.0625f;
}

// ── 서보 직접 PWM (Servo.h 미사용 → Timer1 충돌 없음) ──
// 배경: Servo.h는 Timer1을 점유함 → TCNT1 측정 불가
// 대신 50회 펄스를 직접 생성 (약 1초간 위치 유지)
void servo_set(bool open) {
  uint16_t pulse_us = open ? 1500 : 500;
  pinMode(SERVO_PIN, OUTPUT);
  for (uint8_t i = 0; i < 50; i++) {
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(pulse_us);
    digitalWrite(SERVO_PIN, LOW);
    delay(19);
  }
}

// ── LCD 헬퍼 ──────────────────────────────────────
void lcd_show(const char* l1, const char* l2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(l1);
  lcd.setCursor(0, 1); lcd.print(l2);
}

// ── 버저 ──────────────────────────────────────────
void beep_ok() {
  digitalWrite(BUZZER_PIN, HIGH); delay(200);
  digitalWrite(BUZZER_PIN, LOW);
}
void beep_fail() {
  for (uint8_t i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH); delay(100);
    digitalWrite(BUZZER_PIN, LOW);  delay(100);
  }
}

// ════════════════════════════════════════════════════
// 추상 기반 클래스
// ════════════════════════════════════════════════════
class FirmwareBase {
public:
  // 순수 가상 함수 — 자식 클래스에서 반드시 구현
  virtual bool validate(const char* input) = 0;
  virtual void on_granted() = 0;
  virtual void on_rejected(const char* reason) = 0;

  // 공통 실행 흐름 — 타이밍 측정이 여기 한 곳에만 존재
  void run(const char* input) {
    // ── T_proc: validate() 실행 시간 ──────────────
    uint16_t t0 = TCNT1;
    bool ok = validate(input);
    uint16_t t1 = TCNT1;

    float T_proc = ticks_to_us(t0, t1);

    if (ok) {
      // ── T_act: 액추에이션 명령 시간 ──────────────
      uint16_t t2 = TCNT1;
      on_granted();
      uint16_t t3 = TCNT1;

      float T_act = ticks_to_us(t2, t3);

      Serial.println("ACCEPTED");
      Serial.print("T_proc: "); Serial.print(T_proc); Serial.println(" us");
      Serial.print("T_act:  "); Serial.print(T_act);  Serial.println(" us");

    } else {
      on_rejected("bad_input");

      Serial.println("REJECTED");
      Serial.print("T_proc: "); Serial.print(T_proc); Serial.println(" us");
      // T_act 없음 — 액추에이션 발생하지 않았으므로
    }
  }

  virtual ~FirmwareBase() {}
};

// ════════════════════════════════════════════════════
// 베이스라인: 패스워드 맞으면 무조건 수락 (replay 가능)
// ════════════════════════════════════════════════════
class BaselineFirmware : public FirmwareBase {
public:
  const char* PASSWORD = "password";
  uint8_t failed = 0;
  static const uint8_t MAX_FAIL = 5;
  bool locked = false;

  bool validate(const char* input) override {
    if (locked) return false;
    return (strcmp(input, PASSWORD) == 0);
  }

  void on_granted() override {
    servo_set(true);
    lcd_show("ACCEPTED", "Access granted");
    beep_ok();
    delay(2000);
    servo_set(false);
    lcd_show("BASELINE", "Enter password:");
  }

  void on_rejected(const char* reason) override {
    failed++;
    if (failed >= MAX_FAIL) {
      locked = true;
      lcd_show("LOCKED OUT", "Reset required");
      beep_fail();
    } else {
      char buf[17];
      snprintf(buf, sizeof(buf), "%d chances left", MAX_FAIL - failed);
      lcd_show("WRONG PASSWORD", buf);
      beep_fail();
    }
  }
};

// ════════════════════════════════════════════════════
// 하드닝: 패스워드 + 시퀀스 번호 검증 (replay 차단)
// ════════════════════════════════════════════════════
class HardenedFirmware : public FirmwareBase {
public:
  const char* PASSWORD = "password";
  uint32_t last_seq    = 0;
  bool eeprom_dirty    = false;

  void load_seq() {
    EEPROM.get(EEPROM_ADDR, last_seq);
    if (last_seq == 0xFFFFFFFF) last_seq = 0;
  }

  // 입력 형식: "password:42"  (패스워드:시퀀스번호)
  bool validate(const char* input) override {
    // 콜론으로 분리
    const char* colon = strchr(input, ':');
    if (!colon) return false;

    // 패스워드 부분 추출
    char pw[32];
    uint8_t pw_len = colon - input;
    if (pw_len >= sizeof(pw)) return false;
    strncpy(pw, input, pw_len);
    pw[pw_len] = '\0';

    // 시퀀스 번호 추출
    uint32_t seq = (uint32_t)atol(colon + 1);

    // 검증 1: 패스워드
    if (strcmp(pw, PASSWORD) != 0) return false;

    // 검증 2: 시퀀스 번호
    if (seq == 0)         return false;  // 0 예약
    if (seq <= last_seq)  return false;  // replay 또는 순서 오류

    // 통과
    last_seq     = seq;
    eeprom_dirty = true;
    return true;
  }

  void on_granted() override {
    servo_set(true);
    char buf[17];
    snprintf(buf, sizeof(buf), "SEQ:%lu OK", last_seq);
    lcd_show("ACCEPTED", buf);
    beep_ok();
    delay(2000);
    servo_set(false);
    lcd_show("HARDENED", "pw:seq format");
  }

  void on_rejected(const char* reason) override {
    lcd_show("REJECTED", "Replay/BadInput");
    beep_fail();
    delay(1000);
    lcd_show("HARDENED", "pw:seq format");
  }

  // loop()에서 호출
  void flush_eeprom() {
    if (eeprom_dirty) {
      EEPROM.put(EEPROM_ADDR, last_seq);
      eeprom_dirty = false;
    }
  }
};

// ════════════════════════════════════════════════════
// 전역 객체 및 입력 버퍼
// ════════════════════════════════════════════════════
#ifdef HARDENED_MODE
  HardenedFirmware fw;
#else
  BaselineFirmware fw;
#endif

char    inputBuffer[32];
uint8_t inputLength  = 0;
bool    inputOverflow = false;

// ── setup ─────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  timer1_init();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  lcd.begin(16, 2);

#ifdef HARDENED_MODE
  fw.load_seq();
  char buf[17];
  snprintf(buf, sizeof(buf), "SEQ:%lu", fw.last_seq);
  lcd_show("HARDENED", buf);
  Serial.print("HARDENED MODE | last_seq: ");
  Serial.println(fw.last_seq);
  delay(1500);
  lcd_show("HARDENED", "pw:seq format");
#else
  lcd_show("BASELINE", "Enter password:");
  Serial.println("BASELINE MODE");
#endif

  Serial.println("Ready.");
}

// ── loop ──────────────────────────────────────────
void loop() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\r') continue;

    if (c == '\n') {
      if (inputOverflow) {
        lcd_show("INPUT TOO LONG", "");
        Serial.println("INPUT TOO LONG");
      } else if (inputLength > 0) {
        inputBuffer[inputLength] = '\0';
        fw.run(inputBuffer);   // ← 인터페이스 통일
      }
      inputLength  = 0;
      inputOverflow = false;

    } else {
      if (!inputOverflow) {
        if (inputLength < sizeof(inputBuffer) - 1)
          inputBuffer[inputLength++] = c;
        else
          inputOverflow = true;
      }
    }
  }

#ifdef HARDENED_MODE
  fw.flush_eeprom();
#endif
}
