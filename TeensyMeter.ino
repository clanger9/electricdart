/*
 * Object Oriented CAN example for Teensy 3.6 with Dual CAN buses 
 * By Collin Kidder. Based upon the work of Pawelsky and Teachop
 * 
 * Both buses are set to 500k to show things with a faster bus.
 * The reception of frames in this example is done via callbacks
 * to an object rather than polling. Frames are delivered as they come in.
 */

// OLED setup
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// CAN bus setup
#include <FlexCAN.h>

void OLED_display(float amps = NULL) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.cp437(true);         // Use full 256 char 'Code Page 437' font
    display.setTextSize(2);      // Normal 1:1 pixel scale
    display.setCursor(0, 0);     // Start at top-left corner
    display.println("Current");
    display.println();
    display.setTextSize(3);
    if (amps == NULL) {
      display.println("--- A"); 
      }
    else {
      display.print(amps);
      display.println(" A"); 
      }
    display.display(); 
}

class ExampleClass : public CANListener 
{
public:
   void printFrame(CAN_message_t &frame, int mailbox);
   bool frameHandler(CAN_message_t &frame, int mailbox, uint8_t controller); //overrides the parent version so we can actually do something
};

void ExampleClass::printFrame(CAN_message_t &frame, int mailbox)
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

   if (frame.id == 0x521) {
      long reading = (frame.buf[2] << 24) | (frame.buf[3] << 16) | (frame.buf[4] << 8) | (frame.buf[5]);
      float amps = (float)reading / 1000;
      OLED_display(amps);   
    }
}

bool ExampleClass::frameHandler(CAN_message_t &frame, int mailbox, uint8_t controller)
{
    printFrame(frame, mailbox);

    return true;
}

ExampleClass exampleClass;


// -------------------------------------------------------------
void setup(void)
  {
    // OLED output
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    while (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
      Serial.println("Failed to init SSD1306. Retrying...");
      delay(100);
  }

  OLED_display();
  
  delay(1000);
  Serial.println(F("Hello Teensy Single CAN Receiving Example With Objects."));

  Can0.begin(500000);  

  //if using enable pins on a transceiver they need to be set on
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);

  Can0.attachObj(&exampleClass);
  exampleClass.attachGeneralHandler();
}


// -------------------------------------------------------------
void loop(void)
{
  delay(1000);
  Serial.write('.');
  OLED_display();
}
