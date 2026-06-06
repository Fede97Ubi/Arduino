#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Definizione dei pin
#define PIN_CLK 2
#define PIN_DT 3
#define PIN_SW 4

// Creazione dell'oggetto Encoder
Encoder enc(PIN_CLK, PIN_DT);

// Variabili per la gestione del pulsante
long lastPosition = -999;
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 50;

void setup() {
  Serial.begin(9600);
  delay(2000);
  Serial.println("=== Avvio Sistema ===");

  // Inizializza I2C
  Wire.begin();
  delay(50);

  // Inizializza OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Errore: OLED non trovato"));
    while (true);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Display OK 4");
  display.display();

  // Configura il pin del pulsante
  pinMode(PIN_SW, INPUT_PULLUP);
}

void loop() {
  // Legge la posizione dell'encoder
  long newPosition = enc.read();
  if (newPosition != lastPosition) {
    lastPosition = newPosition;

    // Visualizza la posizione sul display
    display.setCursor(0, 10);
    display.clearDisplay();
    display.print("Posizione: ");
    display.println(newPosition);
    display.display();

    // Stampa la posizione sulla seriale
    Serial.print("Posizione: ");
    Serial.println(newPosition);
  }

  // Gestisce il pulsante con debounce
  if (digitalRead(PIN_SW) == LOW && (millis() - lastButtonPress) > debounceDelay) {
    lastButtonPress = millis();
    Serial.println("Pulsante premuto");
    display.setCursor(0, 20);
    display.print("Pulsante premuto");
    display.display();
  }
}
