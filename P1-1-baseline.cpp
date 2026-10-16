#include <LiquidCrystal.h>
#include <Servo.h>
#include <string.h>
#include <avr/io.h> // used for TCNT1, TCCR1A, TCCR1B

LiquidCrystal lcd(12, 11, 5, 4, 3, 2); // Rs, E, D4, D5, D6, D7

Servo myServo;

const char PASSWORD[] = "123456789";
const uint8_t MAX_ATTEMPTS = 5;

uint8_t failedATTEMPTS = 0;
bool lockedOut = false;

char inputBuffer[32]; // fix the size of the input (buffer) to reduce SRAM fragmentation and to detect overflow
uint8_t inputLength = 0;
bool inputOverflow = false;

void timer1_iv(){
	TCCR1A = 0;
	TCCR1B = (1 << CS10); // prescaler = 1, 분주 없음
						// TCCR1B의 CS10 비트만 세우면 prescaler = 1
						// 1 tick of the Timer1 = 62.5 nanoseconds; max = 6.25 ns * 65535 = 4.096 ms
						// enough range provided for strcmp, servo.write functions
	TCNT1 = 0;
} // initialize Timer1
// use Timer1 instead of using micros() function; micros() has approximately 4 micros resolution.

inline float tToMicros(uint16_t t_start, uint16_t t_end){
	return (uint16_t)(t_end - t_start) * 0.0625f;
} // change ns to ms; 62.5ns = 0.0625ms
// overflow 처리: uint16_t subtraction provides automatic wrap-around

void two_lineLCD(const char* line1, const char* line2){
  lcd.clear();
  lcd.setCursor(0,0); // first line
  lcd.print(line1);
  
  lcd.setCursor(0,1); // move to second line
  lcd.print(line2);
}

int lockedOut_check(bool lockedOut){
  if(lockedOut){
    two_lineLCD("REJECTED", "Reset needed");
    Serial.println("REJECTED \n");
    Serial.println("Locked Out, Reset needed \n");
    return 0;
  } // once input is locked, reject all further input until reset is done
  else {
    uint8_t remain = MAX_ATTEMPTS - failedATTEMPTS;
    
    two_lineLCD("WRONG PASSWORD", "");
    lcd.setCursor(0,1); //강제적으로 다음 줄로 넘어가게 한 뒤, two_lineLCD 함수를 사용해서는 문자열 내부에 %를 사용해 숫자를 넣을 수 없기 때문에 lcd.print() 함수를 사용해야한다.
    lcd.print("("); lcd.print(remain); lcd.print(" chances left)");
    
    Serial.println("WRONG PASSWORD \n");
    Serial.println(remain);
    Serial.println(" chances left \n");
    return 0;
  }
}

void command(const char* input, bool lockedOut){  
  uint16_t t0 = TCNT1; // start the timer t0 after the full input has been received.
  								
  bool accepted = (strcmp(input, PASSWORD) == 0); //strcmp: compare two strings - need string library, 스트링 라이브러리 없이도 할 수 있을까?
  uint16_t t1 = TCNT1; // T_proc timer finish!

  float T_proc = tToMicros(t0, t1);
  
  if(accepted){
    uint16_t t2 = TCNT1; // start the timer for T_act
    myServo.write(90);
	uint16_t t3 = TCNT1; // T_act timer finished

    float T_act = tToMicros(t2, t3);
    
    two_lineLCD("ACCEPTED", "Access granted!");
    Serial.println("ACCEPTED \nAccess granted! \n");

	Serial.print("T_proc: ");
	Serial.print(T_proc);
	Serial.print(" micro seconds \n");

	Serial.print("T_act: ");
	Serial.print(T_act);
	Serial.print(" micro seconds \n");
  } // baseline accepts the same password repeatedly
  else{
    failedATTEMPTS++;
    if(failedATTEMPTS >= MAX_ATTEMPTS) lockedOut = true
	
	lockedOut_check(lockedOut);
   }

	Serial.print("T_proc: ");
	Serial.print(t1-t0);
	Serial.print(" micro seconds \n");
 	// timing output is printed after
	//since it is rejected, T_act, the time calculating how long it takes for the Actuation command to be sent, is not printed
} // caleed when a complete Serial line is received


void setup(){
  Serial.begin(115200);
  timer1_iv();
  lcd.begin(16,2);
  two_lineLCD("baseline ready", "Enter PASSWORD: ");
  
  myServo.attach(9);
  myServo.write(0);
  
  Serial.println("baseline Firmware Ready. \n"); //Timer1 starts! active
  Serial.println("Please Enter the PASSWORD: ");
}

void loop(){
  while(Serial.available() > 0){
    char c = Serial.read();
    
    if(c == '\n'){
      if(inputOverflow){
        two_lineLCD("PASSWORD too long", "Overflow");
        Serial.println("PASSWORD too long, Overflow occurred \n");
        // 다시 입력 화면으로 돌아가게
      } // check overflow
      else if(inputLength > 0){
        inputBuffer[inputLength] = '\0';
        command(inputBuffer, false);
      }
      
      inputLength = 0;
      inputOverflow = false; // command를 호출했기 때문에 initialize, while loop로 다시 돌아가기
    }
    else{
      if(!inputOverflow) {
        if(inputLength < sizeof(inputBuffer) - 1) inputBuffer[inputLength++] = c;  // continue writing password
        
        inputOverflow = true; //enter를 치는 순간 오버플로우 발생했다는 오류 메세지 풀력
      }
    }
  } // writing input without blocking for a newline

}
