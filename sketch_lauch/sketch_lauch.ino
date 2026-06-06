#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>
#include <DHT.h> // Libreria per il sensore di temperatura e umidità

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Pin encoder
#define PIN_CLK 2
#define PIN_DT  3
#define PIN_SW  4

// Pin e Tipo DHT (Termometro)
#define DHTPIN 5      // Collegato al foro numero 5 dell'Arduino
#define DHTTYPE DHT11 // Modello del sensore
DHT dht(DHTPIN, DHTTYPE);

Encoder enc(PIN_CLK, PIN_DT);

long lastPos4x = -999;
long lastPos1x = 0;
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 50;

// Tempistiche per leggere il termometro senza bloccare la manopola
unsigned long previousMillisDHT = 0;
const long intervalDHT = 2000; // Il DHT11 è lento, lo leggiamo ogni 2 secondi

void setup() {
  Serial.begin(9600);
  delay(2000);

  // Inizializzazione Display
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Errore OLED"));
    while (true);
  }

  // Inizializzazione Termometro
  dht.begin();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Sistema Pronto!");
  display.display();

  pinMode(PIN_SW, INPUT_PULLUP);
}

void loop() {
  // --- Lettura Encoder 4x
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

  // --- Lettura Encoder 1x
  long pos1x = pos4x / 4; 
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

  // --- Gestione pulsante (Click)
  bool pressed = !digitalRead(PIN_SW);
  if (pressed && (millis() - lastButtonPress > debounceDelay)) {
    lastButtonPress = millis();
    Serial.println("Click!");
    display.setCursor(0, 30);
    display.fillRect(0, 30, SCREEN_WIDTH, 10, SSD1306_BLACK);
    display.print("Click!");
    display.display();
  }

  // --- Gestione Lettura Termometro (Ogni 2 secondi)
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisDHT >= intervalDHT) {
    previousMillisDHT = currentMillis;

    // Leggiamo i dati dal sensore
    float umidita = dht.readHumidity();
    float temperatura = dht.readTemperature();

    // Verifichiamo se il sensore sta rispondendo correttamente
    if (isnan(umidita) || isnan(temperatura)) {
      Serial.println(F("Errore di lettura dal DHT11!"));
      
      // Scriviamo l'errore sul display per avvisarti
      display.fillRect(0, 45, SCREEN_WIDTH, 19, SSD1306_BLACK);
      display.setCursor(0, 45);
      display.print("Sensore: ERRORE");
      display.display();
    } else {
      // Se è tutto a posto, stampiamo i dati anche sul PC
      Serial.print(F("Umidita': "));
      Serial.print(umidita);
      Serial.print(F("%  Temperatura: "));
      Serial.print(temperatura);
      Serial.println(F("°C"));

      // Puliamo solo la parte bassa dello schermo per non cancellare l'encoder
      display.fillRect(0, 45, SCREEN_WIDTH, 19, SSD1306_BLACK); 
      
      // Mostriamo la Temperatura alla riga 45
      display.setCursor(0, 45);
      display.print("Temp: ");
      display.print(temperatura, 1); // Mostra il valore con un solo decimale
      display.print(" C");

      // Mostriamo l'Umidità alla riga 55
      display.setCursor(0, 55);
      display.print("Umid: ");
      display.print(umidita, 0);     // Mostra il valore intero senza decimali
      display.print(" %");
      
      display.display(); // Aggiorna lo schermo con i nuovi dati
    }
  }

  delay(1); // loop velocissimo per non perdere i passi della manopola
}