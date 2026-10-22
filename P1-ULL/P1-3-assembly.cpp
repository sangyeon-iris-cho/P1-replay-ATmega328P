#include <LiquidCrystal.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <string.h>
// No Servo.h
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

const uint8_t  SERVO_PIN      = 9;
