#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <Servo.h>
#include <string.h>
#include <avr/io.h>

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
Servo myServo;

const char PASS[] = "123456789";
const uint8_t MAX_ATTEMPTS = 10;
const uint16_t EEPROM_ADDR_SEQ = 0; //EEPROM address: 0, 1(2bytes) are used to be written / save sequence numbers inside.

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

const uint8_t BPIN = 8; // active buzzer is connected to the 8th pin of arduino
void buzz_REJECT(){
	for(int i = 0; i<2; i++){
		tone(BPIN, 2000, 100); // 2000Hz, a high toned noise, for 100milliseconds
		delay(200); // stop for 200ms
	}
}
void buzz_RA(){
	for(int i = 0; i<3; i++){
		tone(BPIN, 500, 150); // 500Hz, a way lower toned noise, for 150milliseconds
		delay(200);
	}
}
void buzz_lockedOut(){
	tone(BPIN, 300, 2000); // a low toned noise, for a long time
	delay(2200); //delay time(ms) must be longer than the buzzer's ring time. it starts from the start of the function buzz_lockedOut
}

uint16_t EEPROM_Seq_R(){
	uint8_t lo = EEPROM.read(EEPROM_ADDR_SEQ); // EEPROM[0]
	uint8_t hi = EEPROM.read(EEPROM_ADDR_SEQ + 1); // EEPROM[1]
	// since EEPROM.h's unit is per 1 byte, need to use uint16_t twice to Read & Write
	return ((uint16_t)hi << 8) | lo;
} // inside EEPROM memory, read the sequence number

void EEPROM_Seq_W(uint16_t seq){
	uint8_t lo = seq & 0xFF; // low 8 bits
	uint8_t hi = (seq >> 8) & 0xFF;

	EEPROM.update(EEPROM_ADDR_SEQ, lo); // use update instead of write to save the 횟수 of writing into EEPROM
										// additionally, EEPROM.update() function has a void type return
	EEPROM.update(EEPROM_ADDR_SEQ + 1, hi);
} // update the EEPROM, to 수명 절약, 값이 같으면 쓰지 않음

bool parseSeq(const char* s, uint16_t* out){
  if(s == NULL || s[0] == '\0') return false; // no parsing for empty char array

  if(s[0] == '-') return false; // reject negative value

  for(uint8_t i = 0; s[i] != '\0'; i++){
     if(s[i] < '0' || s[i] > '9') return false; // only for valid numbers, no 특수문자 or alphabets, etc within
     if(i>5) return false; // since 65535, the max number that can be expressed for uint16_t = 2^16 - 1 is 5 digits.
  }
    
  long val = atol(s); // ASCII to long type variable, so no worries for overflow
  if(val < 0 || val > 65535) return false;
	
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
  if(!parseSeq(seqLength, &seqReceived)) return -1; // format error, no negative number
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
    uint16_t t2 = TCNT1; // start the timer for T_act
    myServo.write(90);
	uint16_t t3 = TCNT1; // T_act timer finished

    double T_act = tToMicros(t2, t3);
    
    two_lineLCD("ACCEPTED", "Access granted!");
    Serial.println("ACCEPTED - Access granted!");
	Serial.print("T_proc: ");
	Serial.print(T_proc);
  	Serial.print(" micro seconds \n");
	Serial.print("T_act: ");
	Serial.print(T_act);
	Serial.print(" micro seconds \n");

	Serial.print("SEQ number accepted for this input: ");
	Serial.println(seqReceived);

    SeqNext++;
    EEPROMdirty = true;
  } // right password! accepted
  else if(result == -10) {
	buzz_lockedOut();
	lockedOut_check(); // locked out
  }
  else if(result == -100){ // replay-attack detected!
	buzz_RA();
    two_lineLCD("REPLAY-ATTACK!", "SEQ mismatch");
    Serial.print("REPLAY-ATTACK DETECTED! \n");
    Serial.print("Expected Sequence Number was: ");
    Serial.print(SeqNext);
    Serial.print(" , while current Sequence Number is: ");
    Serial.println(seqReceived);
    
    Serial.print("\n\nT_proc: ");
    Serial.print(T_proc);
    Serial.println(" micro seconds. \n");

    failedATTEMPTS++;
    if(failedATTEMPTS >= MAX_ATTEMPTS){
		lockedOut = true;
		buzz_lockedOut();
	}
  }
  else{
    if(result == -1){
	  buzz_REJECT();
      two_lineLCD("FORMAT ERROR", "Use colon between");
      Serial.println("PASSWORD'S FORMAT IS: PASSWORD:SEQ\n");
	  failedATTEMPTS++
	  if (failedATTEMPTS >= MAX_ATTEMPTS){
		  lockedOut = true;
		  buzz_lockedOut();
	  }
    }
    else{ // result == -2 || == -3; overflow / wrong password typed
	  buzz_REJECT();
      failedATTEMPTS++;
      if(failedATTEMPTS >= MAX_ATTEMPTS){
		  lockedOut = true;
		  buzz_lockedOut();
	  }
	  lockedOut_check();
		
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

  Serial.println("MODE:hardened\n");
  Serial.println("hardened Firmware Ready. \n"); //Timer1 started
  Serial.print("The Next Sequence Number is: "); Serial.println(SeqNext);
  Serial.println("FORMAT: PASS:SEQ");
  Serial.println("Please Enter the PASSWORD: ");
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
		two_lineLCD("PASS too long", "Try again");
        Serial.println("PASSWORD too long, Overflow occurred \n");
		Serial.println("Try again: ");
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


