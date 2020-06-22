// IVT-S meter using CAN-BUS Shield
// electric_dart 2020

#include <SPI.h>
#include "mcp_can.h"

// the cs pin of the version after v1.1 is default to D9
// v0.9b and v1.0 is default D10
const int SPI_CS_PIN = 10;

MCP_CAN CAN(SPI_CS_PIN);                                    // Set CS pin

unsigned char len = 0;
unsigned char buf[8];

void setup() {
    // Set your Serial Monitor to 115200 baud
    Serial.begin(115200);

    // Initialise the CAN bus
    while (CAN_OK != CAN.begin(CAN_1000KBPS)) {            // init can bus
        Serial.println("Failed to initialise CAN bus. Retrying...");
        delay(100);
    }
    Serial.println("CAN bus inititalised.");

    /*
        set receive mask
    */
    CAN.init_Mask(0, 0, 0x7ff);                         // there are 2 masks in mcp2515, you need to set both of them
    CAN.init_Mask(1, 0, 0x7ff);                         // 0x7ff is '11111111111' in binary, so we are checking 11 of the CAN message ID bits  

    /*
        set receive filter
    */
    CAN.init_Filt(0, 0, 0x521);                          // there are 6 filters in mcp2515
    CAN.init_Filt(1, 0, 0x521);                          // 0x521 is the CAN message ID for IVT-S Current value
    CAN.init_Filt(2, 0, 0x521);                          
    CAN.init_Filt(3, 0, 0x521);                          
    CAN.init_Filt(4, 0, 0x521);                          
    CAN.init_Filt(5, 0, 0x521);                          

}

void loop() {
    if (CAN_MSGAVAIL == CAN.checkReceive()) {         // check if data coming
        CAN.readMsgBuf(&len, buf);    // read data,  len: data length, buf: data buf
        unsigned long canId = CAN.getCanId();

        if (canId == 0x521) {
          Serial.print("Data received from IVT_Msg_Result_I");
          Serial.print("\t");
          
          // Convert individual big endian byte values to actual reading 
          long reading = (buf[2] << 24) | (buf[3] << 16) | (buf[4] << 8) | (buf[5]);
          Serial.println(reading);
        }
    }
    // Refresh every 250ms
    delay(250);
}


/*********************************************************************************************************
    END FILE
*********************************************************************************************************/
