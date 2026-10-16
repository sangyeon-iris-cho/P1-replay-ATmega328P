#include <avr/eeprom.h>
#include <LiquidCrystal.h>
#include <Servo.h>
#include <string.h>
#include <avr/io.h>

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
Servo myServo;

const char PASS[] = "123456789";
const uint8_t MAX_ATTEMPTS = 5;
const uint16_t EEPROM_ADDR_SEQ = 0; //EEPROM address to be written, saved
const uint16_t SEQ_W = 1; // saved everytime there's an input

uint8_t failedATTEMPTS = 0;
bool lockedOut = false;

char inputBuffer[40]; // the max digit of the PASS is 24 digits.
uint8_t inputLength = 0;
bool inputOverflow = false;

uint16_t SeqNext; // the sequence number for the next transaction
bool EEPROMdirty = false; // in the void loop() function, use this flag for EEPROM

void timer1_iv(){
  TCCR1A = 0;
  TCCR1B = (1 << CS10); // prescaler = 1; 1 tick is 62.5 nano seconds
  TCNT1 = 0;
} // initialize Timer1

double tToMicros(uint16_t t_start, uint16_t t_end){
  return (uint16_t)(t_end - t_start) * 0.0625;
} // nano second to micro second

void two_lineLCD(const char* line1, const char* line2){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(line1);
  lcd.setCursor(0,1);
  lcd.print(line2);
}

uint16_t EEPROM_Seq_R(){
  return EEPROM_PASS_R((uint16_t*)EEPROM_ADDR_SEQ);
} // inside EEPROM memory, read the sequence number
uint16_t EEPROM_Seq_W(uint16_t seq){
  return EEPROM_PASS_Update((uint16_t)*EEPROM_ADDR_SEQ);
} // update the EEPROM, to 수명 절약, 값이 같으면 쓰지 않음

lockedOut function


int8_t validate(const char* input, double* out_T_proc, uint16_t* out_Seq_received){
  if (lockedOut) return -10; // if the return result is -10, it means that the user is locked out; reset is needed

  const char* colon = strchr(input, ':'); // type parsing, finding colon
  if(colon == NULL) return -1; // if there's no colon located anywhere inside the PASSWORD, the type of the password is wrong.

  uint8_t PWLength = colon - input; // the actual PASSWORD length
  char PW[40];
  if(PWLength >= sizeof(PW)) return -2; // overflow

  memcpy(PW, input, PWLength);
  PW[PWLength] = '\0';

  const char* seqLength = colon + 1; // the sequence number part, after the colon :
  uint16_t seqReceived = (uint16_t)atoi(seqLength);
  *out_Seq_received = seqReceived;

  uint16_t t0 = TCNT1; // Timer1 start!
  bool PWOK = (strcmp(PW, PASS) == 0);
  bool SeqOK = (seqReceived == SeqNext);

  uint16_t t1 = TCNT1;
  *out_T_proc = tToMicros(t0, t1); // T_proc calculation

  if(!PWOK) return -3; // wrong PASSWORD
  if(!SeqOK) return -100; // replay-attack detected!

  return 0; // normal, and right password
}

void command(const char* input){
  double T_proc = 0.0;
  uint16_t seqReceived = 0;

  int8_t result = validate(input, &T_proc, &seqReceived);

  if(result == 0){
    
    
    // right password! accepted

    SeqNext++;
    EEPROMdirty = true;
  }
  else if(result == -10) {
    // locked out
  }
  else if(result == -100){ // replay-attack detected!
    two_lineLCD("REPLAY-ATTACK!", "SEQ different");
    Serial.print("REPLAY-ATTACK DETECTED! \n");
    Serial.print("Expected Sequence Number is: ");
    Serial.print(SeqNext);
    Serial.print(" , while current Sequence Number is: ");
    Serial.println(seqReceived);
    
    Serial.print("\n\nT_proc: ");
    Serial.print(T_proc);
    Serial.println(" micro seconds. \n");
    
    failedATTEMPTS++;
    if(failedATTEMPTS >= MAX_ATTEMPTS) lockedOut = true;
  }
  else{
    if(result == -1){
      two_lineLCD("PASS FORMAT ERROR", "Use colon between");
      Serial.println("PASSWORD'S FORMAT IS: PASSWORD:SEQ\n");
    }
    else{ // result == -2 || == -3; where overflow / wrong password typed
      failedATTEMPTS++;
      if(failedATTEMPTS >= MAX_ATTEMPTS) lockedOut = true;
      lockedOut 함수
      Serial.print("T_proc: ");
      Serial.print(T_proc);
      Serial.println(" micro seconds. \n");
    }
  }
}



void setup(){
  Serial.begin(115200);
  timer1_iv();
  lcd.begin(16,2);

  SeqNext = EEPROM_Seq_R(); // EEPROM에서 Sequence number 복원하기

  if(SeqNext == 0xFFFF){
    SeqNext = 0;
    EEPROM_Seq_W(0);
  } // booting the EEPROM for the first time, if it's 0xFFFF, initialize

  myServo.attach(9);
  myServo.write(0);

  레디
  
}

void loop(){
  if(EEPROMdirty){
    EEPROM_Seq_W(SeqNext);
    EEPROMdirty = false;
  } // use this dirty flag to prevent blocking, inside the loop

  while(Serial.available() > 0){
    char c = Serial.read();

    if(c == '\n'){
      if(inputOverflow){
        two_lineLCD()
      }
      else if(inputLength > 0){
        inputBuffer[inputLength] = '\0';
        command(inputBuffer);
      }

      inputLength = 0;
      inputOverflow = false;
    }
    else{
      if(!inputOverflow){
        if(inputLength < sizeof(inputBuffer) - 1) inputBuffer[inputLength++] = c;
        else inputOverflow = true;
      }
    }
  }
}





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
