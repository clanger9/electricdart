// CAN bus state-of-charge meter for Teensy 3.x, CAN bus shield and retro-style 4-digit 7-segment display
// Includes automatic LED brightness control
// 
// Usage: send integer number over CAN bus to configured address
// Values between 0 and 1000 will be displayed as "  0.0" to "100 "
// Anything else will display "----"
//
// electric_dart 2021

// define display
const int digits = 4; // 4 digit display
const int segments = 7; // 7 segment display
const boolean common_anode = 1; // 1=common anode 0=common cathode
const int pin_segment[segments] = {13, 14, 15, 16, 17, 18, 19}; // a,b,c,d,e,f,g pins
const int pin_dp = 20; // decimal point pin
const int pin_digit[digits] = {12, 11, 10, 9}; // digit pins, left to right
const int refresh_hz = 50; // minimum 40Hz to avoid flicker

// define display characters
const int zero[segments] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, LOW};
const int one[segments] = {LOW, HIGH, HIGH, LOW, LOW, LOW, LOW};
const int two[segments] = {HIGH, HIGH, LOW, HIGH, HIGH, LOW, HIGH};
const int three[segments] = {HIGH, HIGH, HIGH, HIGH, LOW, LOW, HIGH};
const int four[segments] = {LOW, HIGH, HIGH, LOW, LOW, HIGH, HIGH};
const int five[segments] = {HIGH, LOW, HIGH, HIGH, LOW, HIGH, HIGH};
const int six[segments] = {HIGH, LOW, HIGH, HIGH, HIGH, HIGH, HIGH};
const int seven[segments] = {HIGH, HIGH, HIGH, LOW, LOW, LOW, LOW};
const int eight[segments] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
const int nine[segments] = {HIGH, HIGH, HIGH, LOW, LOW, HIGH, HIGH};
const int blank[segments] = {LOW, LOW, LOW, LOW, LOW, LOW, LOW};
const int dash[segments] = {LOW, LOW, LOW, LOW, LOW, LOW, HIGH};
// add additional characters here if required
const int* character[12] = {zero, one, two, three, four, five, six, seven, eight, nine, blank, dash};

// photocell parameters
int pin_photocell = 32; // photocell pin
int dark = 500; // adjust for your photocell
int light = 1000; // adjust for your photocell

// CAN bus setup
#include <FlexCAN.h>
const long can_id = 0x350; // set this to match your CAN bus ID
const long can_speed = 500000; // set this to match your CAN bus speed
const long can_timeout = 10; // seconds to wait without data before showing error

// internal variables
int refresh_interval = 1000 / (digits * refresh_hz); // milliseconds
int on_time; // microseconds
unsigned long currentMillis;
unsigned long currentMicros;
unsigned long next_refresh = 0;
unsigned long next_blank = 0;
unsigned long next_timeout = 0;
int current_digit = 0;
int reading = -1;
int photocell_reading;
int led_brightness;

void clearDisplay()
{
  for (int i = 0; i < digits; ++i)
    {
      digitalWrite(pin_digit[i], HIGH ^ common_anode);
    }
}

void writeDisplayDigit(int digit, int value, boolean decimal)
// digit: numbered left-to-right, beginning at zero
// value: the required character to display from character[] array
// decimal: set this to 1 to switch the decimal point on
{
  for (int i = 0; i < segments; ++i)
  {
    digitalWrite(pin_segment[i], character[value][i] ^ common_anode);
  }
  digitalWrite(pin_dp, decimal ^ common_anode);
  digitalWrite(pin_digit[digit], LOW ^ common_anode);
}

void writeDisplay(int digit, int value) // customise this section according to what you want to display
{
    if (value == 1000) { // display "100 " if the input value is 1000
      switch (digit) {
        case 0:
        writeDisplayDigit(0, 1, 0);
        break;
        case 1:
        writeDisplayDigit(1, 0, 0);
        break;
        case 2:
        writeDisplayDigit(2, 0, 0);
        break;
      }
    }  
    else if (value >= 100 and value < 1000) { // display " 99 " to " 10 " for input values from 999 to 100
      switch (digit) {
        case 1:
        writeDisplayDigit(1, value / 100, 0);
        break;
        case 2:
        writeDisplayDigit(2, (value / 10) % 10, 0);
        break;
      }
    }
    else if (value >= 0 and value < 100) { // display "  9.9" to "  0.0" for input values from 99 to 0
      switch (digit) {
        case 2:
        writeDisplayDigit(2, value / 10, 1);
        break;
        case 3:
        writeDisplayDigit(3, value % 10, 0);
        break;
      }    
    }
    else { // display "----" for anything else
        writeDisplayDigit(digit, 11, 0);
    }
}

class CANlistenerClass : public CANListener 
{
public:
   void printFrame(CAN_message_t &frame, int mailbox);
   bool frameHandler(CAN_message_t &frame, int mailbox, uint8_t controller); //overrides the parent version so we can actually do something
};

void CANlistenerClass::printFrame(CAN_message_t &frame, int mailbox)
{
   Serial.print("ID: ");
   Serial.print(frame.id, HEX);
   Serial.print(" Data: ");
   for (int c = 0; c < frame.len; c++) 
   {
      Serial.print(frame.buf[c], HEX);
      Serial.write(' ');
   }
   Serial.write('\r');
   Serial.write('\n');

   if (frame.id == can_id) {
      // Matching CAN bus frame arrived!
      // now piece it together e.g.
      reading = (frame.buf[6] << 8) | (frame.buf[7]);
      next_timeout = currentMillis + (can_timeout * 1000);
    }
}

bool CANlistenerClass::frameHandler(CAN_message_t &frame, int mailbox, uint8_t controller)
{
    printFrame(frame, mailbox);
    return true;
}

CANlistenerClass myClass;

void setup() {
  Serial.begin(9600);//initialise serial communications at 9600 bps     

  delay(1000);
  Serial.println(F("Hello Teensy Single CAN Receiving Example With Objects."));

  Can0.begin(can_speed);  

  //if using enable pins on a transceiver they need to be set on
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);

  Can0.attachObj(&myClass);
  myClass.attachGeneralHandler();

  //initialise segement pins
  for (int i = 0; i < 7; ++i)
    {
      pinMode(pin_segment[i], OUTPUT);
    }
    
  //initialise decimal point pin
  pinMode(pin_dp, OUTPUT);
  digitalWrite(pin_dp, LOW ^ common_anode);

  //initialise digit pins
  for (int i = 0; i < digits; ++i)
    {
      pinMode(pin_digit[i], OUTPUT);
    }
  clearDisplay();
}

void loop() {

  // add extra code here

  // pot input for testing without CAN bus
  //reading = analogRead(pin_pot); 
  //reading = map(reading, 10, 1023, 0, 1000);
  //
  //

  // take a timestamp
  currentMillis = millis();
  currentMicros = micros();

  // calculate required LED brightness score (1-10) and set time to remain on (in microseconds)
  photocell_reading = analogRead(pin_photocell);  
  photocell_reading = constrain(photocell_reading, dark, light);
  led_brightness = map(photocell_reading, dark, light, 1, 10);
  on_time = led_brightness * refresh_interval * 100; // microseconds

  // timeout
  if (currentMillis > next_timeout) {
    reading = -1;
  }

  // is it time to refresh display?
  if (currentMillis > next_refresh) {
    next_refresh = currentMillis + refresh_interval; // set the time in milliseconds for the next refresh
    next_blank = currentMicros + on_time; // set the time in microseconds for LEDs to remain on (for dimming)
    writeDisplay(current_digit, reading); // multiplexed display, so do one digit at at time
    ++current_digit; // we'll do the next digit on the next pass
    if (current_digit > digits - 1 ) { current_digit = 0; } // all digits done? wrap around to first digit again.
  }

  // is it time to switch the LEDs off?
  if (currentMicros > next_blank ) {
    clearDisplay();
  }
}
