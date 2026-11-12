/*
 *
 * Connect the RN2xx3 as follows:
 * RN2xx3 -- Arduino
 * Uart TX -- 6
 * Uart RX -- 5
 * Reset -- 4
 * 3.3V -- VCC
 * Gnd -- Gnd
 *
 */
#include <rn2xx3.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(6, 5); // RX, TX

//create an instance of the rn2xx3 library,
//giving the software serial as port to use
rn2xx3 myLora(mySerial);

// the setup routine runs once when you press reset:
void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(57600); //serial port to computer
  mySerial.begin(9600); //serial port to radio
  Serial.println("Startup");

  initialize_radio();
  delay(2000);
}

void initialize_radio()
{
  //reset rn2483
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  delay(500);
  digitalWrite(4, HIGH);

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

  /*
   * OTAA: initOTAA(String AppEUI, String AppKey);
   * If you are using OTAA, paste the example code from the TTN console here:
   */
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

// the loop routine runs over and over again forever:
void loop()
{

    Serial.println("TXing");
    myLora.tx("!"); //one byte, blocking function

    delay(20000);
}


