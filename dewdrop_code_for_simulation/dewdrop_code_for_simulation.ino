// Den kode jeg har kørt for at teste i simulationen, den trækker fra dewdrop.h
// Beregninger på størrelse: 100x143 = 14.300 pixels
//en byte repræsentere otte pixels = 100/8 = 12,5 bytes = 13*143=1859 bytes for alle tre ansigter = 1859x3 = 5.577 bytes
// Tjek for om det er de libraries vi skal bruge op mod koden du har lavet Frederikke



#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include "dewdrop.h"

// E-paper pin configuration
#define EPD_CS    10
#define EPD_DC     9
#define EPD_RST    8
#define EPD_BUSY   7

// E-paper display: 200 x 200 pixels
// Paged drawing to save SRAM
GxEPD2_BW<GxEPD2_154_D67, 24> display(
  GxEPD2_154_D67(
    EPD_CS,
    EPD_DC,
    EPD_RST,
    EPD_BUSY
  )
);

// Dewdrop bitmap configuration
#define DEWDROP_WIDTH  100
#define DEWDROP_HEIGHT 143

// Bitmap position
#define DEWDROP_X 50
#define DEWDROP_Y 54

// Text position
#define TEXT_Y 18

// Emotional states
enum Mood {
  HAPPY = 1,
  NEUTRAL = 2,
  SAD = 3
};

Mood currentMood = NEUTRAL;

// Draw centered text
void drawMessage(const char* message) {
  display.setTextColor(GxEPD_BLACK);
  display.setTextSize(2);
  display.setTextWrap(false);

  int16_t x1, y1;
  uint16_t w, h;

  display.getTextBounds(
    message, 0, 0, &x1, &y1, &w, &h
  );

  int16_t textX = (display.width() - w) / 2 - x1;

  display.setCursor(textX, TEXT_Y);
  display.print(message);
}

// Display Dewdrop and message
void showDewdrop(Mood mood, const char* message) {

  display.setFullWindow();
  display.firstPage();

  do {
    display.fillScreen(GxEPD_WHITE);

    // Draw text
    drawMessage(message);

    // Draw selected bitmap
    switch (mood) {

      case HAPPY:
        display.drawBitmap(
          DEWDROP_X, DEWDROP_Y,
          dewdropHappy,
          DEWDROP_WIDTH,
          DEWDROP_HEIGHT,
          GxEPD_BLACK
        );
        break;

      case NEUTRAL:
        display.drawBitmap(
          DEWDROP_X, DEWDROP_Y,
          dewdropNeutral,
          DEWDROP_WIDTH,
          DEWDROP_HEIGHT,
          GxEPD_BLACK
        );
        break;

      case SAD:
        display.drawBitmap(
          DEWDROP_X, DEWDROP_Y,
          dewdropSad,
          DEWDROP_WIDTH,
          DEWDROP_HEIGHT,
          GxEPD_BLACK
        );
        break;
    }

  } while (display.nextPage());

  display.powerOff();

  currentMood = mood;
}

// Handle incoming status - bruger vi når vi skal koble til LoRAWAN
void handleDownlink(uint8_t status) {

  switch (status) {

    case 1:
      showDewdrop(HAPPY, "Godmorgen");
      Serial.println(F("Dewdrop: HAPPY"));
      break;

    case 2:
      showDewdrop(NEUTRAL, "Hej med dig");
      Serial.println(F("Dewdrop: NEUTRAL"));
      break;

    case 3:
      showDewdrop(SAD, "Tid til vand");
      Serial.println(F("Dewdrop: SAD"));
      break;
  }
}

// Initialisation
void setup() {
  Serial.begin(9600);

  display.init(0, true, 2, false);
  display.setRotation(0);

  showDewdrop(NEUTRAL, "Godmorgen");

  Serial.println(F("Dewdrop is ready!"));
  Serial.println(F("1 = Happy"));
  Serial.println(F("2 = Neutral"));
  Serial.println(F("3 = Sad"));
}

// Main loop
void loop() {

  while (Serial.available() > 0) {

    char input = Serial.read();

    if (input >= '1' && input <= '3') {
      handleDownlink(input - '0');
    }
  }
}
