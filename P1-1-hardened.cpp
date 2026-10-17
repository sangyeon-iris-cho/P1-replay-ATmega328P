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

bool parseSeq(const char* s, uint16_t* out){
  if(s == NULL || s[0] == '\0') return false; // no parsing for empty char array

  if(s[0] == '-') return false; // reject negative value

  for(uint8_t i = 0; s[i] != '\0'; i++){
     if(s[i] < '0' || s[i] > '9') return false; // only for valid numbers, no 특수문자 or alphabets, etc within
     if(i>5) return false; // since 65535 is 5 digits.
  }
    
  long val = atol(s); // ASCII to long type variable, so no worries for overflow
  if(val < 0 || val > 65535) return false; //65535 is a number

  *out = (uint16_t)val;
  return true;
} // parsing function instead of atio - uint16_t seqReceived = (uint16_t)atoi(seqLength);
  
void lockedOut_check(){
  if(lockedOut){
    two_lineLCD("REJECTED", "Reset needed");
    Serial.println("REJECTED \n");
    Serial.println("Locked Out, Reset needed \n");
    return;
  } // once input is locked, reject all further input until reset is done
  else {
    uint8_t remain = MAX_ATTEMPTS - failedATTEMPTS;
    
    two_lineLCD("WRONG PASSWORD", "");
    lcd.setCursor(0,1); //강제적으로 다음 줄로 넘어가게 한 뒤, two_lineLCD 함수를 사용해서는 문자열 내부에 %를 사용해 숫자를 넣을 수 없기 때문에 lcd.print() 함수를 사용해야한다.
    lcd.print("("); lcd.print(remain); lcd.print(" chances left)");
    
    Serial.println("WRONG PASSWORD \n");
    Serial.println(remain);
    Serial.println(" chances left \n");
    return;
  }
}

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

  uint16_t seqReceived = 0;
  if(!parseSeq(seqStr, &seqReceived)) return -1; // format error, no negative number
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


