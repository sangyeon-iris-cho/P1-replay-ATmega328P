// Project 4: Assembly-Optimized Hardened Firmware
// ATmega328P — Timer1 direct register, inline ASM strcmp, inline ASM EEPROM R/W
// Servo.h removed → manual PWM pulse via delayMicroseconds()

#include <LiquidCrystal.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <string.h>

// ─── Hardware ────────────────────────────────────────────────────────────────
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

const uint8_t  SERVO_PIN      = 9;
const uint8_t  BPIN           = 8;
const uint16_t EEPROM_ADDR    = 0;   // 2 bytes: word at address 0

// ─── Config ──────────────────────────────────────────────────────────────────
const char     PASS[]         = "123456789";
const uint8_t  MAX_ATTEMPTS   = 10;

// ─── State ───────────────────────────────────────────────────────────────────
uint8_t  failedATTEMPTS = 0;
bool     lockedOut      = false;
uint16_t SeqNext        = 0;
bool     EEPROMdirty    = false;

char     inputBuffer[40];
uint8_t  inputLength   = 0;
bool     inputOverflow = false;

// ─── Timer1: normal mode, prescaler=1, 62.5 ns/tick ─────────────────────────
inline void timer1_init() __attribute__((always_inline));
inline void timer1_init() {
    TCCR1A = 0;
    TCCR1B = (1 << CS10);
    TCNT1  = 0;
}

inline double tToMicros(uint16_t t0, uint16_t t1) __attribute__((always_inline));
inline double tToMicros(uint16_t t0, uint16_t t1) {
    return (uint16_t)(t1 - t0) * 0.0625;
}

// ─── LCD helper ──────────────────────────────────────────────────────────────
inline void two_lineLCD(const char* l1, const char* l2) __attribute__((always_inline));
inline void two_lineLCD(const char* l1, const char* l2) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(l1);
    lcd.setCursor(0, 1); lcd.print(l2);
}

// ─── Servo: manual PWM, no Servo.h → Timer1 stays in normal mode ─────────────
// pulse_us = 1000 (0°) … 2000 (180°), period = 20000 us
inline void servo_write(uint8_t angle) __attribute__((always_inline));
inline void servo_write(uint8_t angle) {
    uint16_t pulse_us = 1000 + (uint16_t)angle * 1000 / 180;
    // pulse_us = map(angle, 0, 180, 544, 2400);
                    // standard servo: 0 degree = 544us, 90 degree = 1500us, 180 degree = 2400us
    for (uint8_t i = 0; i < 25; i++) {   // ~25 pulses ≈ 0.5 s stabilization
        digitalWrite(SERVO_PIN, HIGH);
        delayMicroseconds(pulse_us);
        digitalWrite(SERVO_PIN, LOW);
        delayMicroseconds(20000 - pulse_us); // the need for this delay is to ensure a total period of 20 ms (50 Hz) for the servo signal, which is standard for most hobby servos.
    }
} 

// ─── Buzzer ──────────────────────────────────────────────────────────────────
inline void buzz_REJECT() __attribute__((always_inline));
inline void buzz_REJECT() {
    for (int i = 0; i < 2; i++) { tone(BPIN, 2000, 100); delay(200); }
}

inline void buzz_RA() __attribute__((always_inline));
inline void buzz_RA() {
    for (int i = 0; i < 3; i++) { tone(BPIN, 500, 150); delay(200); }
}

inline void buzz_lockedOut() __attribute__((always_inline));
inline void buzz_lockedOut() {
    tone(BPIN, 300, 2000); delay(2200);
}

// ─── ASM: EEPROM word read (avr/eeprom.h 래핑 + inline ASM 명시) ──────────────
// eeprom_read_word() 내부를 직접 인라인 어셈블리로 구현
inline uint16_t EEPROM_Seq_R() __attribute__((always_inline));
inline uint16_t EEPROM_Seq_R() {
    uint16_t result;
    asm volatile (
        // EEAR = address register (EEARH:EEARL)
        "out %[eearl], %A[addr]     \n"  // EEARL = lo(EEPROM_ADDR)
        "out %[eearh], %B[addr]     \n"  // EEARH = hi(EEPROM_ADDR)
        "sbi %[eecr], %[eere]       \n"  // EERE = 1 → trigger read
        "in  %A[res], %[eedr]       \n"  // lo byte ← EEDR

        // next address = EEPROM_ADDR + 1
        "ldi r24, lo8(%[addr1])     \n"
        "ldi r25, hi8(%[addr1])     \n"
        "out %[eearl], r24          \n"
        "out %[eearh], r25          \n"
        "sbi %[eecr], %[eere]       \n"
        "in  %B[res], %[eedr]       \n"  // hi byte ← EEDR

        : [res]   "=r" (result)
        : [addr]  "r"  ((uint16_t)EEPROM_ADDR),
          [addr1] "M"  (EEPROM_ADDR + 1),
          [eearl] "I"  (_SFR_IO_ADDR(EEARL)),
          [eearh] "I"  (_SFR_IO_ADDR(EEARH)),
          [eedr]  "I"  (_SFR_IO_ADDR(EEDR)),
          [eecr]  "I"  (_SFR_IO_ADDR(EECR)),
          [eere]  "I"  (EERE)
        : "r24", "r25"
    );
    return result;
}

// ─── ASM: EEPROM word write ───────────────────────────────────────────────────
inline void EEPROM_Seq_W(uint16_t seq) __attribute__((always_inline));
inline void EEPROM_Seq_W(uint16_t seq) {
    // Write lo byte
    asm volatile (
        "cli                        \n"  // disable interrupts during write
        "out %[eearl], %A[addr]     \n"
        "out %[eearh], %B[addr]     \n"
        "out %[eedr],  %A[val]      \n"  // data = lo byte
        "sbi %[eecr],  %[eempe]     \n"  // EEMPE = 1 (master write enable)
        "sbi %[eecr],  %[eepe]      \n"  // EEPE  = 1 (start write)
        "eeprom_w0:                 \n"
        "sbic %[eecr], %[eepe]      \n"  // wait until EEPE clears
        "rjmp eeprom_w0             \n"
        "sei                        \n"  // re-enable interrupts
        :
        : [addr]  "r"  ((uint16_t)EEPROM_ADDR),
          [val]   "r"  (seq),
          [eearl] "I"  (_SFR_IO_ADDR(EEARL)),
          [eearh] "I"  (_SFR_IO_ADDR(EEARH)),
          [eedr]  "I"  (_SFR_IO_ADDR(EEDR)),
          [eecr]  "I"  (_SFR_IO_ADDR(EECR)),
          [eempe] "I"  (EEMPE),
          [eepe]  "I"  (EEPE)
        :
    );
    // Write hi byte
    asm volatile (
        "cli                        \n"
        "ldi r24, lo8(%[addr1])     \n"
        "ldi r25, hi8(%[addr1])     \n"
        "out %[eearl], r24          \n"
        "out %[eearh], r25          \n"
        "out %[eedr],  %B[val]      \n"  // data = hi byte
        "sbi %[eecr],  %[eempe]     \n"
        "sbi %[eecr],  %[eepe]      \n"
        "eeprom_w1:                 \n"
        "sbic %[eecr], %[eepe]      \n"
        "rjmp eeprom_w1             \n"
        "sei                        \n"
        :
        : [addr1] "M"  (EEPROM_ADDR + 1),
          [val]   "r"  (seq),
          [eearl] "I"  (_SFR_IO_ADDR(EEARL)),
          [eearh] "I"  (_SFR_IO_ADDR(EEARH)),
          [eedr]  "I"  (_SFR_IO_ADDR(EEDR)),
          [eecr]  "I"  (_SFR_IO_ADDR(EECR)),
          [eempe] "I"  (EEMPE),
          [eepe]  "I"  (EEPE)
        : "r24", "r25"
    );
}

// ─── ASM: strcmp replacement ──────────────────────────────────────────────────
// Returns true if s1 == s2 (null-terminated)
// Z pointer = s1, X pointer = s2
inline bool asm_streq(const char* s1, const char* s2) __attribute__((always_inline));
inline bool asm_streq(const char* s1, const char* s2) {
    uint8_t match = 1;
    asm volatile (
        "asm_cmp_loop:          \n"
        "    ld  r24, Z+        \n"   // r24 = *s1++
        "    ld  r25, X+        \n"   // r25 = *s2++
        "    cp  r24, r25       \n"   // compare bytes
        "    brne asm_cmp_fail  \n"   // mismatch → fail
        "    tst r24            \n"   // null terminator reached?
        "    brne asm_cmp_loop  \n"   // not yet → continue
        "    rjmp asm_cmp_done  \n"   // both null → equal
        "asm_cmp_fail:          \n"
        "    clr %[match]       \n"   // match = 0
        "asm_cmp_done:          \n"
        : [match] "+r" (match)
        : "z" (s1), "x" (s2)
        : "r24", "r25"
    );
    return (match != 0);
}

// ─── parseSeq ────────────────────────────────────────────────────────────────
inline bool parseSeq(const char* s, uint16_t* out) __attribute__((always_inline));
inline bool parseSeq(const char* s, uint16_t* out) {
    if (s == NULL || s[0] == '\0') return false;
    if (s[0] == '-') return false;
    for (uint8_t i = 0; s[i] != '\0'; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        if (i > 5) return false;
    }
    long val = atol(s);
    if (val < 0 || val > 65535) return false;
    *out = (uint16_t)val;
    return true;
}

// ─── lockedOut_check ──────────────────────────────────────────────────────────
inline void lockedOut_check() __attribute__((always_inline));
inline void lockedOut_check() {
    if (lockedOut) {
        two_lineLCD("REJECTED", "Reset needed");
        Serial.println("REJECTED");
        Serial.println("Locked Out - Reset needed");
        return;
    }
    uint8_t remain = MAX_ATTEMPTS - failedATTEMPTS;
    two_lineLCD("WRONG PASSWORD", "");
    lcd.setCursor(0, 1);
    lcd.print("("); lcd.print(remain); lcd.print(" left)");
    Serial.println("WRONG PASSWORD");
    Serial.print(remain); Serial.println(" chances left");
}

// ─── validate ────────────────────────────────────────────────────────────────
int8_t __attribute__((optimize("O3"))) validate(
    const char* input, double* out_T_proc, uint16_t* out_seq)
{
    if (lockedOut) return -10;

    const char* colon = strchr(input, ':');
    if (colon == NULL) return -1;

    uint8_t pwLen = colon - input;
    char PW[40];
    if (pwLen >= sizeof(PW)) return -2;
    memcpy(PW, input, pwLen);
    PW[pwLen] = '\0';

    const char* seqStr = colon + 1;
    uint16_t seqReceived = 0;
    if (!parseSeq(seqStr, &seqReceived)) return -1;
    *out_seq = seqReceived;

    // ── Timed section: ASM strcmp ──────────────────────────────────────────
    uint16_t t0 = TCNT1;
    bool PWOK  = asm_streq(PW, PASS);       // ASM strcmp
    bool SeqOK = (seqReceived == SeqNext);
    uint16_t t1 = TCNT1;
    // ──────────────────────────────────────────────────────────────────────

    *out_T_proc = tToMicros(t0, t1);

    if (!PWOK)  return -3;
    if (!SeqOK) return -100;
    return 0;
}

// ─── command ─────────────────────────────────────────────────────────────────
void command(const char* input) {
    // RESET:admin — lockout clear + SEQ reset to 0
    if (asm_streq(input, "RESET:admin")) {
        failedATTEMPTS = 0;
        lockedOut      = false;
        SeqNext        = 0;
        EEPROM_Seq_W(0);
        Serial.println("RESET:OK");
        two_lineLCD("RESET OK", "SEQ=0");
        return;
    }

bool constTimeEqual(const char* a, const char* b, size_t len){
  uint8_t diff = 0;
  for(size_t i = 0; i < len; i++){
    diff |= (uint8_t)a[i] ^ (uint8_t)b[i]; // 다르면 diff에 흔적이 남음
  }
  return diff == 0; // 끝까지 다 돌고 나서야 판정
} // string compare
/*uint16_t t0 = TCNT1;
  const uint8_t PASS_LEN = sizeof(PASS) - 1; // null 제외 9
  bool PWOK = (PWLength == PASS_LEN) && constTimeEqual(PW, PASS, PASS_LEN);
  bool SeqOK = (seqReceived == SeqNext);
*/
    double   T_proc = 0.0;
    uint16_t seqReceived = 0;
    int8_t   result = validate(input, &T_proc, &seqReceived);

    if (result == 0) {
        uint16_t t2 = TCNT1;
        servo_write(90);                     // Timer1 safe — no Servo.h, instead of myServo.write(90)
        uint16_t t3 = TCNT1;
        double T_act = tToMicros(t2, t3);

        two_lineLCD("ACCEPTED", "Access granted!");
        Serial.println("ACCEPTED - Access granted!");
        Serial.print("T_proc: "); Serial.print(T_proc);  Serial.println(" us");
        Serial.print("T_act: ");  Serial.print(T_act);   Serial.println(" us");
        Serial.print("SEQ accepted: "); Serial.println(seqReceived);

        SeqNext++;
        EEPROMdirty = true;
    }
    else if (result == -10) {
        buzz_lockedOut();
        lockedOut_check();
    }
    else if (result == -100) {
        buzz_RA();
        two_lineLCD("REPLAY-ATTACK!", "SEQ mismatch");
        Serial.println("REPLAY-ATTACK DETECTED!");
        Serial.print("Expected SEQ: "); Serial.print(SeqNext);
        Serial.print(" | Received: "); Serial.println(seqReceived);
        Serial.print("T_proc: "); Serial.print(T_proc); Serial.println(" us");

        failedATTEMPTS++;
        if (failedATTEMPTS >= MAX_ATTEMPTS) {
            lockedOut = true;
            buzz_lockedOut();
        }
    }
    else {
        if (result == -1) {
            buzz_REJECT();
            two_lineLCD("FORMAT ERROR", "Use PASS:SEQ");
            Serial.println("FORMAT ERROR - Use PASS:SEQ");
        }
        else {  // -2 overflow, -3 wrong pw
            buzz_REJECT();
            failedATTEMPTS++;
            if (failedATTEMPTS >= MAX_ATTEMPTS) {
                lockedOut = true;
                buzz_lockedOut();
            }
            lockedOut_check();
            Serial.print("T_proc: "); Serial.print(T_proc); Serial.println(" us");
        }
    }
}



void setup() {
    Serial.begin(115200);
    timer1_init();
    lcd.begin(16, 2);
    pinMode(SERVO_PIN, OUTPUT); // myServo.attach(SERVO_PIN == 9) with manual PWM
    digitalWrite(SERVO_PIN, LOW); // myServo.write(0 degree)

    SeqNext = EEPROM_Seq_R();
    if (SeqNext == 0xFFFF) {
        SeqNext = 0;
        EEPROM_Seq_W(0);
    }

    servo_write(0);   // initial position

    Serial.println("MODE:hardened_asm");
    Serial.print("Expected SEQ:"); Serial.println(SeqNext);
    Serial.println("FORMAT: PASS:SEQ");
    Serial.println("Hardened-ASM Firmware Ready.");
}

// ─── loop ────────────────────────────────────────────────────────────────────
void loop() {
    if (EEPROMdirty) {
        EEPROM_Seq_W(SeqNext);
        EEPROMdirty = false;
    }

    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n') {
            if (inputOverflow) {
                two_lineLCD("PASS too long", "Try again");
                Serial.println("OVERFLOW - Try again");
            }
            else if (inputLength > 0) {
                inputBuffer[inputLength] = '\0';
                command(inputBuffer);
            }
            inputLength   = 0;
            inputOverflow = false;
        }
        else {
            if (!inputOverflow) {
                if (inputLength < sizeof(inputBuffer) - 1)
                    inputBuffer[inputLength++] = c;
                else
                    inputOverflow = true;
            }
        }
    }
}
