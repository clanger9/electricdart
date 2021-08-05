// demo: CAN-BUS Shield, send data
// loovee@seeed.cc


#include <SPI.h>

#define CAN_2515
// #define CAN_2518FD

// Set SPI CS Pin according to your hardware

#if defined(SEEED_WIO_TERMINAL) && defined(CAN_2518FD)
// For Wio Terminal w/ MCP2518FD RPi Hat：
// Channel 0 SPI_CS Pin: BCM 8
// Channel 1 SPI_CS Pin: BCM 7
// Interupt Pin: BCM25
const int SPI_CS_PIN  = BCM8;
const int CAN_INT_PIN = BCM25;
#else

// For Arduino MCP2515 Hat:
// the cs pin of the version after v1.1 is default to D9
// v0.9b and v1.0 is default D10
const int SPI_CS_PIN = 10;
const int CAN_INT_PIN = 2;
#endif


#ifdef CAN_2518FD
#include "mcp2518fd_can.h"
mcp2518fd CAN(SPI_CS_PIN); // Set CS pin
#endif

#ifdef CAN_2515
#include "mcp2515_can.h"
mcp2515_can CAN(SPI_CS_PIN); // Set CS pin
#endif

int sensorPin = A0;    // select the input pin for the potentiometer
int sensorValue = 0;  // variable to store the value coming from the sensor
int node_id = 0x350;

void setup() {
    Serial.begin(9600);
    while(!Serial){};

    while (CAN_OK != CAN.begin(CAN_1000KBPS)) { // for some reason I have to set CAN_1000KBPS to get it to send at 500kbps 
        Serial.println("CAN init fail, retrying...");
        delay(100);
    }
    Serial.println("CAN init OK");
}

unsigned char stmp[8] = {0, 0, 0, 0, 0, 0, 0, 0};
void loop() {

    // read the value from the sensor:
    sensorValue = analogRead(sensorPin); 
    stmp[7] = lowByte(sensorValue);
    stmp[6] = highByte(sensorValue);

    CAN.sendMsgBuf(node_id, 0, 8, stmp);
    delay(100);                       // send data per 100ms
    Serial.print("Sensor data: ");
    Serial.print(sensorValue);    
    Serial.print("  CAN send node ID 0x");
    Serial.print(node_id, HEX);
    Serial.print(": ");
    for (int i=0; i<=7; ++i) { 
      Serial.print(stmp[i]);
      Serial.print(" ");
    }
    Serial.println();
}

// END FILE
