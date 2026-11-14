/* Connect the e-paper as follows:
 * e-paper -- Arduino
 * Busy -- 7
 * RST -- 8
 * DC -- 9
 * CS -- 10
 * CLK -- 13
 * DIN -- 11
 * Vcc -- VCC
 * Gnd -- Gnd
 *
 * Connect the RN2xx3 as follows:
 * RN2xx3 -- Arduino
 * Uart TX -- 6
 * Uart RX -- 5
 * Reset -- 4
 * Vcc -- 3.3V
 * Gnd -- Gnd
 */

#define TX_PIN 6
#define RX_PIN 5
#define reset_PIN 4

#define X_POS 48
#define Y_POS 52
#define IMAGE_WIDTH 96
#define IMAGE_HEIGHT 96

#include <SPI.h>
#include "epd1in54_V2.h"
#include "imagedata.h"
#include "epdpaint.h"
#include <stdio.h>
#include <rn2xx3.h>
#include <SoftwareSerial.h>

Epd epd;
unsigned char image[1024];
Paint paint(image, 0, 0);

#define COLORED     0
#define UNCOLORED   1

SoftwareSerial mySerial(TX_PIN, RX_PIN); // RX, TX

//create an instance of the rn2xx3 library,
//giving the software serial as port to use
rn2xx3 myLora(mySerial);

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(57600);
  mySerial.begin(9600); //serial port to radio
  Serial.println("Startup");

  epd.LDirInit();
  epd.Clear();
}

void initialize_radio()
{
  //reset rn2483
  pinMode(reset_PIN, OUTPUT);
  digitalWrite(reset_PIN, LOW);
  delay(500);
  digitalWrite(reset_PIN, HIGH);

  delay(100); //wait for the RN2xx3's startup message
  mySerial.flush();

  //Autobaud the rn2483 module to 9600. The default would otherwise be 57600.
  myLora.autobaud();

  //check communication with radio
  String hweui = myLora.hweui();
  while(hweui.length() != 16)
  {
    Serial.println("Communication with RN2xx3 unsuccessful. Power cycle the board.");
    Serial.println(hweui);
    delay(10000);
    hweui = myLora.hweui();
  }


  //configure your keys and join the network
  Serial.println("Trying to join TTN");
  bool join_result = false;

  const char *appEui = "193C3D528C9C2FFF";
  const char *appKey = "8B2F38DF86BDA7881502499D456FD9C9";

  join_result = myLora.initOTAA(appEui, appKey);


  while(!join_result)
  {
    Serial.println("Unable to join. Are your keys correct, and do you have TTN coverage?");
    delay(60000); //delay a minute before retry
    join_result = myLora.init();
  }
  Serial.println("Successfully joined TTN");

}

void loop()
{
epd.LDirInit();
// Set the static background first
//epd.DisplayPartBaseImage(IMAGE_DATA);

if (Serial.available() > 0) {
        int input = Serial.parseInt();

        // Clear remaining serial characters
        while (Serial.available() > 0) {
            Serial.read();
        }

        const unsigned char* selectedImage = nullptr;
        const char* imageName = "";

        switch (input) {
            case 0:
                selectedImage = SAD;
                imageName = "SAD";
                break;

            case 1:
                selectedImage = NEUTRAL;
                imageName = "NEUTRAL";
                break;

            case 2:
                selectedImage = HAPPY;
                imageName = "HAPPY";
                break;

            default:
                Serial.println("Invalid input! Enter 0, 1, or 2.");
                return;
        }

        epd.SetFrameMemory(
            selectedImage,
            X_POS,
            Y_POS,
            IMAGE_WIDTH,
            IMAGE_HEIGHT
        );

        epd.DisplayFrame();

        Serial.print("Displaying ");
        Serial.println(imageName);
    }

  
  //paint.SetWidth(200);
  //paint.SetHeight(24);
  //paint.Clear(COLORED);
  //
  //paint.DrawStringAt(30, 4, "Good morning!", &Font16, UNCOLORED);
  //epd.SetFrameMemory(paint.GetImage(), 0, 10, paint.GetWidth(), paint.GetHeight());
  //epd.DisplayPartFrame();
//
  //epd.Sleep();
//
  //delay(1000);
//
  //epd.LDirInit();
//
  //paint.SetWidth(200);
  //paint.SetHeight(24);
  //paint.Clear(COLORED);
//
  //paint.Clear(COLORED);
  //paint.DrawStringAt(40, 4, "Good night!", &Font16, UNCOLORED);
  //epd.SetFrameMemory(paint.GetImage(), 0, 10, paint.GetWidth(), paint.GetHeight());
  //epd.DisplayPartFrame();
//
  //epd.Sleep();
  //delay(1000);
}
