#include <EEPROM.h>

#define SRC_SERIAL 1
#define CMD_OPEN 0x01
#define CMD_CLOSE 0x02
#define EEPROM_ADDR_SEQ 0 //EEPROM address is in the range of 0~3, since uint32_t seq is 4 bytes

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

void timer1_iv(){
  TCCR1A = 0;
  TCCR1B = (1 << CS10);
  TCNT1 = 0;
} // initialize Timer1

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
