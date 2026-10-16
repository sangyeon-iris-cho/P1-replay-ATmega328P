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


---------------------------------------------------------------------------------------------
  hardened
  
struct Acommand{ 
  uint8_t source;
  uint8_t opcode;
  uint32_t seq;
  uint16_t timestamp; // for Timer1
};

class securityModule{
public:
  virtual bool validate(const Acommand& ev) = 0;
  virtual void report(const Acommand& ev) = 0;
};

// IR channel 대신 class : public securityModule{}
class serialChannel : public securityModule{
public:
  uint32_t seq_end = 0;
  bool EEPROM_dirty = false;

  void loadFromEEPROM(){
    EEPROM.get(EEPROM_ADDR_SEQ, seq_end);
    if(seq_end == 0xFFFFFFFF) seq_end = 0; //to initialize the unused EEPROM
  }

  void report(const Acommand& ev) override{
    Serial.print("Serial | SEQ: ");
    Serial.print(ev.seq);
    Serial.print(" | LAST: ");
    Serial.print(seq_end);
    Serial.println(" | REJECTED \n");
  }

  bool validate(const Acommand& ev) override {
    if(ev.seq == 0) return false; //reserve for seq 0
    if(ev.seq <= seq_end){
      report(ev);
      return false;
    }

    seq_end = ev.seq;
    EEPROM_dirty = true; //in order to prevent blocking
    return true;
  } // polymorphism; same name defined inside securityModule, but different operation

  void flushEEPROM(){
    if(EEPROM_dirty){
      EEPROM.put(EEPROM_ADDR_SEQ, seq_end);
      EEPROM_dirty = false;
    }
  }// this is called inside the void loop() function, to 분산하다 blocking to loop
};

//new channel object define
serialChannel SChannel;
uint16_t time_start;


void servo_write(uint8_t degree){
  OCR1A = 2000 + (uint16_t)degree * 1000 / 90;
} // control the servo motor directly by using the registers
  // by defining this new function by ourself, we can calculate the exact, more precise timing than using the Servo.h functions.
// Timer1을 서보 PWM으로 재구성하면 측정 타이머와 충돌
// → 여기서는 간단히 analogWrite 방식 유지 (논문 구현 간소화)
// 실제 직접 제어는 Timer1 Phase 3에서 분리

// ── 시리얼 명령 파서 ──────────────────────
// 형식: "CMD:OPEN:SEQ:42\n"
bool parseSerial(CommandEvent& ev) {
  if (!Serial.available()) return false;
  String line = Serial.readStringUntil('\n');
  line.trim();

  // 파싱
  int idx1 = line.indexOf(':');
  int idx2 = line.indexOf(':', idx1+1);
  int idx3 = line.indexOf(':', idx2+1);
  if (idx1 < 0 || idx2 < 0 || idx3 < 0) return false;

  String cmdStr = line.substring(idx1+1, idx2);
  String seqStr = line.substring(idx3+1);

  ev.source = SRC_SERIAL;
  ev.seq    = seqStr.toInt();
  ev.opcode = (cmdStr == "OPEN") ? CMD_OPEN : CMD_CLOSE;
  ev.timestamp = TCNT1;
  return true;
}

    
void setup() {
  Serial.begin(115200);
  timer1_init();
  serialChan.loadFromEEPROM();
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("READY");
}

void loop() {
  uint16_t t0, t1, t2, t3;

  // ── IR 이벤트 처리 ──────────────────────
  if (IrReceiver.decode()) {
    t0 = TCNT1;

    CommandEvent ev;
    ev.source    = SRC_IR;
    ev.timestamp = t0;

    uint32_t code = IrReceiver.decodedIRData.decodedRawData;
    IrReceiver.resume();

    t1 = TCNT1;  // T_proc 끝

    // 명령 매핑 (본인 코드로 수정)
    if      (code == 0xFF30CF) ev.opcode = CMD_OPEN;
    else if (code == 0xFF18E7) ev.opcode = CMD_CLOSE;
    else goto ir_done;

    t2 = TCNT1;  // T_dec 시작

    if (irChan.validate(ev)) {
      t3 = TCNT1;  // T_act 시작
      // 서보 제어
      // (생략 — Phase 1-2의 servo.write() 그대로)

      float T_proc = (t1-t0)*0.0625f;
      float T_dec  = (t2-t1)*0.0625f;
      float T_act  = (t3-t2)*0.0625f;
      Serial.print("IR|T_proc:"); Serial.print(T_proc);
      Serial.print("|T_dec:");    Serial.print(T_dec);
      Serial.print("|T_act:");    Serial.println(T_act);
    }
    ir_done:;
  }

  // ── 시리얼 이벤트 처리 ──────────────────
  CommandEvent sev;
  if (parseSerial(sev)) {
    t0 = TCNT1;

    bool ok = serialChan.validate(sev);

    t1 = TCNT1;

    if (ok) {
      Serial.print("SEQ:");
      Serial.print(sev.seq);
      Serial.println(":ACCEPTED");
    }
    // rejected는 validate() 안에서 이미 출력됨
  }

  // ── EEPROM 지연 쓰기 ────────────────────
  serialChan.flushEEPROM();
}
