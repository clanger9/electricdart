// CAN bus state-of-charge meter for Teensy 3.x, CAN bus shield and 4-digit 7-segment display
// Includes automatic LED brightness control
//
// Usage: send integer number over CAN bus to configured address
// Values between 0 and 1000 will be displayed as "  0.0" to "100 "
// The display flashes "  0.0" when it reaches zero
// Anything else (or no data) will be displayed as "----"
//
// electric_dart 2021

// define display
// values below are for 3461BS-1 4 digit common anode 7 segment display 
const int ledDigits = 4; // 4 digits
const int ledSegments = 7; // 7 segments 
const int pinSegment[ledSegments] = {13, 14, 15, 16, 17, 18, 19}; // pins for a,b,c,d,e,f,g segments
const int pinDecimalPoint = 20; // pin for decimal point
const int pinDigit[ledDigits] = {12, 11, 10, 9}; // common digit pins, left to right
const boolean ledIsCommonAnode = 1; // 1=common anode 0=common cathode
const int ledRefreshHz = 50; // minimum 40Hz to avoid flicker
const int ledFlashHz = 2; // for flashing display

// define display characters
const int zero[ledSegments] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, LOW};
const int one[ledSegments] = {LOW, HIGH, HIGH, LOW, LOW, LOW, LOW};
const int two[ledSegments] = {HIGH, HIGH, LOW, HIGH, HIGH, LOW, HIGH};
const int three[ledSegments] = {HIGH, HIGH, HIGH, HIGH, LOW, LOW, HIGH};
const int four[ledSegments] = {LOW, HIGH, HIGH, LOW, LOW, HIGH, HIGH};
const int five[ledSegments] = {HIGH, LOW, HIGH, HIGH, LOW, HIGH, HIGH};
const int six[ledSegments] = {HIGH, LOW, HIGH, HIGH, HIGH, HIGH, HIGH};
const int seven[ledSegments] = {HIGH, HIGH, HIGH, LOW, LOW, LOW, LOW};
const int eight[ledSegments] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
const int nine[ledSegments] = {HIGH, HIGH, HIGH, LOW, LOW, HIGH, HIGH};
const int blank[ledSegments] = {LOW, LOW, LOW, LOW, LOW, LOW, LOW};
const int dash[ledSegments] = {LOW, LOW, LOW, LOW, LOW, LOW, HIGH};
// add additional characters here if required
const int* character[12] = {zero, one, two, three, four, five, six, seven, eight, nine, blank, dash};

// photocell parameters
int pinPhotocell = 32; // photocell pin
int photocellDark = 500; // adjust dark level for your photocell
int photocellLight = 1000; // adjust light level for your photocell

// CAN bus setup
#include <FlexCAN.h>
const long canID = 0x350; // set this to match your CAN bus ID
const long canSpeed = 500000; // set this to match your CAN bus speed
const long canTimeout = 10; // seconds to wait without data before showing error

// internal variables
int ledRefreshMilliseconds = 1000 / (ledDigits * ledRefreshHz); // milliseconds
int ledOnMicroseconds; // microseconds
unsigned long nowMilliseconds;
unsigned long nowMicroseconds;
unsigned long nextRefreshMilliseconds = 0;
unsigned long nextBlankMicroseconds = 0;
unsigned long nextTimeoutMilliseconds = 0;
unsigned long nextFlashMilliseconds = 0;
boolean ledFlashState = 1;
int digitSelect = 0;
int reading = -1;
int photocellReading;
int ledBrightness; // score 1 to 10

void clearDisplay()
{
  for (int i = 0; i < ledDigits; ++i)
  {
    digitalWrite(pinDigit[i], HIGH ^ ledIsCommonAnode);
  }
}

void writeDisplayDigit(int digit, int value, boolean decimal)
// digit: numbered left-to-right, beginning at zero
// value: the character to display from character[] array
// decimal: set this to 1 to switch the decimal point on
{
  for (int i = 0; i < ledSegments; ++i)
  {
    digitalWrite(pinSegment[i], character[value][i] ^ ledIsCommonAnode);
  }
  digitalWrite(pinDecimalPoint, decimal ^ ledIsCommonAnode);
  digitalWrite(pinDigit[digit], LOW ^ ledIsCommonAnode);
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
  else if (value >= 1 and value < 100) { // display "  9.9" to "  0.1" for input values from 99 to 1
    switch (digit) {
      case 2:
        writeDisplayDigit(2, value / 10, 1);
        break;
      case 3:
        writeDisplayDigit(3, value % 10, 0);
        break;
    }
  }
  else if (value == 0) { // display flashing "  0.0" for input value 0
    if (nowMilliseconds > nextFlashMilliseconds) {
      nextFlashMilliseconds = nowMilliseconds + ( 1000 / ledFlashHz );
      ledFlashState = ledFlashState ^ 1;
    }
    if (ledFlashState == 1) { 
      switch (digit) {
        case 2:
          writeDisplayDigit(2, 0, 1);
          break;
        case 3:
          writeDisplayDigit(3, 0, 0);
          break;
      }
    }
    else {
      switch (digit) {
        case 2:
          writeDisplayDigit(2, 10, 1);
          break;
        case 3:
          writeDisplayDigit(3, 10, 0);
          break;
      }
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

  if (frame.id == canID) {
    // Matching CAN bus frame arrived!
    // now piece it together e.g.
    reading = (frame.buf[6] << 8) | (frame.buf[7]);
    nextTimeoutMilliseconds = nowMilliseconds + (canTimeout * 1000); // set next timeout
  }
}

bool CANlistenerClass::frameHandler(CAN_message_t &frame, int mailbox, uint8_t controller)
{
  printFrame(frame, mailbox);
  return true;
}

CANlistenerClass myClass;

void setup() {
  Serial.begin(9600); //initialise serial communications at 9600 bps


  // Initialise CAN bus
  delay(1000); // allow CAN hardware to stabilise
  Can0.begin(canSpeed);
  //if using enable pins on a transceiver they need to be set on
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  Can0.attachObj(&myClass);
  myClass.attachGeneralHandler();

  // Initialise LED display

  // segement pins
  for (int i = 0; i < ledSegments; ++i)
  {
    pinMode(pinSegment[i], OUTPUT);
  }

  // decimal point pin
  pinMode(pinDecimalPoint, OUTPUT);
  digitalWrite(pinDecimalPoint, LOW ^ ledIsCommonAnode);

  // digit pins
  for (int i = 0; i < ledDigits; ++i)
  {
    pinMode(pinDigit[i], OUTPUT);
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
  nowMilliseconds = millis();
  nowMicroseconds = micros();

  // calculate required LED brightness score (1-10) and set time to remain on (in microseconds) as a proportion of the refresh interval
  photocellReading = analogRead(pinPhotocell);
  photocellReading = constrain(photocellReading, photocellDark, photocellLight);
  ledBrightness = map(photocellReading, photocellDark, photocellLight, 1, 10);
  ledOnMicroseconds = ledBrightness * ledRefreshMilliseconds * 100; // microseconds

  // timeout if no data received
  if (nowMilliseconds > nextTimeoutMilliseconds) {
    reading = -1; // display "----"
  }

  // is it time to refresh display?
  if (nowMilliseconds > nextRefreshMilliseconds) {
    nextRefreshMilliseconds = nowMilliseconds + ledRefreshMilliseconds; // set the time in milliseconds for the next refresh
    nextBlankMicroseconds = nowMicroseconds + ledOnMicroseconds; // set the time in microseconds for LEDs to remain on (for dimming)
    writeDisplay(digitSelect, reading); // multiplexed display, so enable one digit at at time
    ++digitSelect; // we'll do the next digit on the next pass
    if (digitSelect > ledDigits - 1 ) { // all digits done? 
      digitSelect = 0;  // wrap around to first digit again.
    }
  }

  // is it time to switch the LEDs off?
  if (nowMicroseconds > nextBlankMicroseconds ) {
    clearDisplay();
  }
}
