#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define PIN_CLK 2
#define PIN_DT 3
#define PIN_SW 4

Encoder enc(PIN_CLK, PIN_DT);

long lastPosition4x = -999;  // conta ogni transizione (4x)
long lastPosition1x = 0;      // conta solo un passo per scatto
long stepCounter = 0;          // per raggruppare i 4 segnali

void setup() {
  Serial.begin(9600);
  delay(2000);

  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Display OK 2");
  display.display();

  pinMode(PIN_SW, INPUT_PULLUP);
}

void loop() {
  // --- 4x counting
  long pos4x = enc.read();
  if (pos4x != lastPosition4x) {
    lastPosition4x = pos4x;
    Serial.print("4x Encoder: ");
    Serial.println(pos4x);

    display.setCursor(0,10);
    display.fillRect(0,10,SCREEN_WIDTH,10,SSD1306_BLACK);
    display.print("4x: ");
    display.println(pos4x);
    display.display();

    // --- 1x counting (ogni 4 incrementi)
    stepCounter++;
    if (stepCounter >= 4) {
      if (pos4x > 0) lastPosition1x++;
      else lastPosition1x--;
      stepCounter = 0;

      Serial.print("1x Encoder: ");
      Serial.println(lastPosition1x);

      display.setCursor(0,20);
      display.fillRect(0,20,SCREEN_WIDTH,10,SSD1306_BLACK);
      display.print("1x: ");
      display.println(lastPosition1x);
      display.display();
    }
  }

  // --- Pulsante
  bool pressed = !digitalRead(PIN_SW);
  if (pressed) {
    Serial.println("Click!");
    display.setCursor(0,30);
    display.fillRect(0,30,SCREEN_WIDTH,10,SSD1306_BLACK);
    display.print("Click!");
    display.display();
    delay(200); // semplice debounce
  }

  delay(1); // loop veloce
}
