#include <LiquidCrystal.h>
#include <Servo.h>
#include <string.h>
#include <avr/io.h> // used for TCNT1, TCCR1A, TCCR1B (timer1)

LiquidCrystal lcd(12, 11, 5, 4, 3, 2); // Rs, E, D4, D5, D6, D7

Servo myServo;

const char PASSWORD[] = "123456789";

char inputBuffer[32]; // fix the size of the input (buffer) to reduce SRAM fragmentation and to detect overflow
uint8_t inputLength = 0;
bool inputOverflow = false;

void timer1_iv(){
	TCCR1A = 0; // normal counter mode, not PWM
	TCCR1B = (1 << CS10); // set the CS10 bit in TCCR1B register => prescaler = 1, prescaler isn't used
						// the timer clock runs at the exact same frequency as the io clock, timer++ on every one clock cycle
						// 1 tick of the Timer1 = 62.5 nanoseconds; max = 6.25 ns * 65535 = 4.096 ms
						// enough range provided for strcmp, servo.write functions
	TCNT1 = 0;
} // initialize Timer1
// use Timer1 instead of using micros() function; micros() has approximately 4 micros resolution.

double tToMicros(uint16_t t_start, uint16_t t_end){
	return (uint16_t)(t_end - t_start) * 0.0625;
} // change ns to ms; 62.5ns = 0.0625ms
// overflow handling: uint16_t subtraction provides automatic wrap-around

void two_lineLCD(const char* line1, const char* line2){
  lcd.clear();
  lcd.setCursor(0,0); // first line
  lcd.print(line1);
  
  lcd.setCursor(0,1); // move to second line
  lcd.print(line2);
}

void command(const char* input){  
  uint16_t t0 = TCNT1; // start the timer t0 after the full input has been received.
  								
  bool accepted = (strcmp(input, PASSWORD) == 0); //strcmp: compare two strings - need string library
  uint16_t t1 = TCNT1; // T_proc timer finish!

  double T_proc = tToMicros(t0, t1);
  
  if(accepted){
    uint16_t t2 = TCNT1; // start the timer for T_act
    myServo.write(90);
	uint16_t t3 = TCNT1; // T_act timer finished

    double T_act = tToMicros(t2, t3);
    
    two_lineLCD("ACCEPTED", "Access granted!");
    Serial.println("ACCEPTED \nAccess granted! \n");

	Serial.print("T_proc: ");
	Serial.print(T_proc);
	Serial.print(" micro seconds \n");

	Serial.print("T_act: ");
	Serial.print(T_act);
	Serial.print(" micro seconds \n");
  } // baseline accepts the same password repeatedly
	else {
	two_LCD("WRONG PASS", "TRY AGAIN");
	Serial.print("WRONG PASSWORD, Try again");
	Serial.print("T_proc: ");
	Serial.print(T_proc);
	Serial.print(" micro seconds \n");
 	// timing output is printed after
	//since it is rejected, T_act, the time calculating how long it takes for the Actuation command to be sent, is not printed
	}
} // caleed when a complete Serial line is received


void setup(){
  Serial.begin(115200);
  timer1_iv();
  lcd.begin(16,2);
  two_lineLCD("baseline ready", "Enter PASSWORD: ");
  
  myServo.attach(9);
  myServo.write(0);

  Serial.println("MODE:baseline\n");
  Serial.println("baseline Firmware Ready."); //Timer1 starts! active
  Serial.println("Please Enter the PASSWORD: ");
}

void loop(){
  while(Serial.available() > 0){
    char c = Serial.read();
    
    if(c == '\n'){
      if(inputOverflow){
        two_lineLCD("PASSWORD too long", "Try again");
        Serial.println("PASSWORD too long, Overflow occurred \n");
		Serial.println("Try again: ");
      } // check overflow, if so, exit the conditional block -> initialize -> while loop starts again, restarting the input
      else if(inputLength > 0){
        inputBuffer[inputLength] = '\0';
        command(inputBuffer);
      }
      
      inputLength = 0;
      inputOverflow = false; // command called: initialize, start over the while loop
    }
    else{
      if(!inputOverflow) {
        if(inputLength < sizeof(inputBuffer) - 1){
			inputBuffer[inputLength++] = c;  // continue writing password
		} else {
        inputOverflow = true; // print overflow message as soon as enter pressed, true only when buffer is full
      	}
	  }
    }
  } // writing input without blocking for a newline

}
