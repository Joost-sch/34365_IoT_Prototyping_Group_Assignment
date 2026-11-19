#include <SPI.h>
#include <rn2xx3.h>
#include <SoftwareSerial.h>
#include "epd1in54_V2.h"
#include "imagedata.h"

#define LORA_TX 6
#define LORA_RX 5
#define LORA_RESET 4

#define X_POS 48
#define Y_POS 52
#define IMAGE_WIDTH 104
#define IMAGE_HEIGHT 143

SoftwareSerial loraSerial(LORA_TX, LORA_RX);
rn2xx3 lora(loraSerial);
Epd epd;

const char *appEui = "B2D337899DD08D35";
const char *appKey = "D56AE2F720709BC53FC5F8CF6E7178FE";

const uint8_t UPLINK_PORT = 2;
const unsigned long SEND_INTERVAL = 60000;

uint8_t petState = 1;

void setup()
{
  Serial.begin(57600);
  loraSerial.begin(9600);
  showPet();
  startRadio();
  joinTTN();
}

void loop()
{
  sendUplink();
  delay(SEND_INTERVAL);
}

void showPet()
{
  const unsigned char *image = dewdropNeutral;
  if (petState == 0) image = dewdropSad;
  if (petState == 2) image = dewdropHappy;

  epd.LDirInit();
  epd.Clear();
  epd.SetFrameMemory(image, X_POS, Y_POS, IMAGE_WIDTH, IMAGE_HEIGHT);
  epd.DisplayFrame();
  epd.Sleep();

  Serial.print("Showing pet state ");
  Serial.println(petState);
}

void startRadio()
{
  pinMode(LORA_RESET, OUTPUT);
  digitalWrite(LORA_RESET, LOW);
  delay(500);
  digitalWrite(LORA_RESET, HIGH);
  delay(100);
  loraSerial.flush();

  lora.autobaud();
  while (lora.hweui().length() != 16)
  {
    Serial.println("No answer from RN2483, retrying");
    delay(10000);
    lora.autobaud();
  }

  Serial.print("DevEUI: ");
  Serial.println(lora.hweui());
}

void joinTTN()
{
  Serial.println("Joining TTN");
  while (!lora.initOTAA(appEui, appKey))
  {
    Serial.println("Join failed, retrying");
    delay(20000);
  }
  Serial.println("Joined");
  lora.sendRawCommand("mac set adr off");
}

void sendUplink()
{
  char hex[3];
  sprintf(hex, "%02X", petState);

  Serial.print("Uplink: ");
  Serial.println(hex);

  String command = String("mac tx uncnf ") + String(UPLINK_PORT) + " ";
  TX_RETURN_TYPE result = lora.txCommand(command, hex, false);

  if (result == TX_WITH_RX)
  {
    String downlink = lora.getRx();
    Serial.print("Downlink: ");
    Serial.println(downlink);

    if (downlink.length() >= 2)
    {
      uint8_t newState = strtol(downlink.substring(0, 2).c_str(), NULL, 16);
      if (newState <= 2 && newState != petState)
      {
        petState = newState;
        showPet();
      }
    }
  }
  else if (result == TX_FAIL)
  {
    Serial.println("Uplink failed, rejoining");
    startRadio();
    joinTTN();
  }
}
