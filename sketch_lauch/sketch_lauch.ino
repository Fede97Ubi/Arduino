#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Pin encoder
#define PIN_CLK 2
#define PIN_DT  3
#define PIN_SW  4

Encoder enc(PIN_CLK, PIN_DT);

// Variabili per le due modalità
long lastPos4x = -999;
long lastPos1x = 0;

// Pulsante debounce
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 50;

void setup() {
  Serial.begin(9600);
  delay(2000);

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Errore OLED"));
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Display OK 2");
  display.display();

  pinMode(PIN_SW, INPUT_PULLUP);
}

void loop() {
  // --- 4x counting (tutti i fronti)
  long pos4x = enc.read();
  if (pos4x != lastPos4x) {
    lastPos4x = pos4x;

    Serial.print("4x Encoder: ");
    Serial.println(pos4x);

    display.setCursor(0, 10);
    display.fillRect(0, 10, SCREEN_WIDTH, 10, SSD1306_BLACK);
    display.print("4x: ");
    display.println(pos4x);
    display.display();
  }

  // --- 1x counting (solo uno step per passo meccanico)
  long pos1x = pos4x / 4; // legge solo multipli di 4
  if (pos1x != lastPos1x) {
    lastPos1x = pos1x;

    Serial.print("1x Encoder: ");
    Serial.println(pos1x);

    display.setCursor(0, 20);
    display.fillRect(0, 20, SCREEN_WIDTH, 10, SSD1306_BLACK);
    display.print("1x: ");
    display.println(pos1x);
    display.display();
  } 

  // --- Gestione pulsante
  bool pressed = !digitalRead(PIN_SW);
  if (pressed && (millis() - lastButtonPress > debounceDelay)) {
    lastButtonPress = millis();
    Serial.println("Click!");
    display.setCursor(0, 30);
    display.fillRect(0, 30, SCREEN_WIDTH, 10, SSD1306_BLACK);
    display.print("Click!");
    display.display();
  }

  delay(1); // loop veloce
}
