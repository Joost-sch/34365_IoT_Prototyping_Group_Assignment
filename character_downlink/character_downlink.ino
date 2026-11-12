#include <rn2xx3.h>
#include <SoftwareSerial.h>

SoftwareSerial loraSerial(10, 11);
rn2xx3 lora(loraSerial);

const char *appEui = "B2D337899DD08D35";
const char *appKey = "D56AE2F720709BC53FC5F8CF6E7178FE";

const uint8_t UPLINK_PORT = 2;
const unsigned long SEND_INTERVAL = 60000;


uint8_t petState = 1;

void setup()
{
  Serial.begin(57600);
  loraSerial.begin(9600);
  startRadio();
  joinTTN();
}

void loop()
{
  sendUplink();
  delay(SEND_INTERVAL);
}

void startRadio()
{
  pinMode(12, OUTPUT);
  digitalWrite(12, LOW);
  delay(500);
  digitalWrite(12, HIGH);
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
  lora.setDR(2);
  lora.sendRawCommand("mac set adr off");
}

void sendUplink()
{
  uint8_t uplink[] = {petState};
  String hex = "";
  char buffer[3];
  for (uint8_t i = 0; i < sizeof(uplink); i++)
  {
    sprintf(buffer, "%02X", uplink[i]);
    hex += buffer;
  }

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
      petState = strtol(downlink.substring(0, 2).c_str(), NULL, 16);
      Serial.print("Pet state: ");
      Serial.println(petState);
    }
    lora.sendRawCommand("mac set adr off");
  }
  else if (result == TX_FAIL)
  {
    Serial.println("Uplink failed, rejoining");
    startRadio();
    joinTTN();
  }

}
