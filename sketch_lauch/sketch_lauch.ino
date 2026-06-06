#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>
#include <DHT.h> 

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Pin encoder
#define PIN_CLK 2
#define PIN_DT  3
#define PIN_SW  4

// Pin e Tipo DHT (Termometro)
#define DHTPIN 5      
#define DHTTYPE DHT11 
DHT dht(DHTPIN, DHTTYPE);

Encoder enc(PIN_CLK, PIN_DT);

long lastPos4x = -999;
long lastPos1x = 0;
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 250; 

// --- VARIABILE PER GESTIONE PAGINE ---
int paginaCorrente = 0; 
const int TOTALE_PAGINE = 2;

// --- MEMORIZZAZIONE DATI GRAFICO ---
const int MAX_PUNTI = 30; 
float storiciTemperatura[MAX_PUNTI];
int puntiMemorizzati = 0;

float temperaturaAttuale = 0.0;
float umiditaAttuale = 0.0;

// Tempistiche
unsigned long previousMillisDHT = 0;
const long intervalDHT = 2000; 

unsigned long previousMillisGrafico = 0;
const long intervalGrafico = 10000; 

// Funzione sicura per calcolare la RAM libera rimasta nell'ATmega328P
int getRamLibera() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

void aggiungiDatoGrafico(float nuovaTemp) {
  if (puntiMemorizzati < MAX_PUNTI) {
    storiciTemperatura[puntiMemorizzati] = nuovaTemp;
    puntiMemorizzati++;
  } else {
    for (int i = 0; i < MAX_PUNTI - 1; i++) {
      storiciTemperatura[i] = storiciTemperatura[i + 1];
    }
    storiciTemperatura[MAX_PUNTI - 1] = nuovaTemp;
  }
}

void setup() {
  Serial.begin(9600);
  delay(2000);

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Errore OLED"));
    while (true);
  }

  dht.begin();

  for (int i = 0; i < MAX_PUNTI; i++) {
    storiciTemperatura[i] = 0.0;
  }

  display.clearDisplay();
  display.display();

  pinMode(PIN_SW, INPUT_PULLUP);
}

void loop() {
  unsigned long currentMillis = millis();

  // --- Gestione cambio pagina col Pulsante
  bool pressed = !digitalRead(PIN_SW);
  if (pressed && (currentMillis - lastButtonPress > debounceDelay)) {
    lastButtonPress = currentMillis;
    paginaCorrente = (paginaCorrente + 1) % TOTALE_PAGINE;
    display.clearDisplay(); 
    display.display();
  }

  // --- Lettura Encoder
  long pos4x = enc.read();
  long pos1x = pos4x / 4; 

  // --- Gestione Lettura Sensore (Ogni 2 secondi)
  if (currentMillis - previousMillisDHT >= intervalDHT) {
    previousMillisDHT = currentMillis;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      temperaturaAttuale = t;
      umiditaAttuale = h;
    }
  }

  // --- Gestione Salvataggio Storico (Ogni 10 secondi)
  if (currentMillis - previousMillisGrafico >= intervalGrafico) {
    previousMillisGrafico = currentMillis;
    if (temperaturaAttuale != 0.0) {
      aggiungiDatoGrafico(temperaturaAttuale);
    }
  }

  // ========================================================
  // RENDERING DELLE SCHERMATE
  // ========================================================
  
  if (paginaCorrente == 0) {
    // --------------------------------------------------------
    // PAGINA 1: ENCODER (SX) + MEMORIA RAM (DX) + METEO (BASSO)
    // --------------------------------------------------------
    display.fillRect(0, 0, SCREEN_WIDTH, 64, SSD1306_BLACK); 
    
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // --- LATO SINISTRO: Giri Manopola (fino a colonna X = 60)
    display.setCursor(0, 0);
    display.print("4x: "); display.print(pos4x);
    display.setCursor(0, 12);
    display.print("1x: "); display.print(pos1x);
    
    // --- LINEA VERTICALE DI DIVISIONE (A metà schermo, X = 64, alta da Y=0 a Y=22)
    display.drawFastVLine(64, 0, 22, SSD1306_WHITE);

    // --- LATO DESTRO: Controllo Memoria RAM (da colonna X = 68)
    int ramLibera = getRamLibera();
    int ramTotale = 2048; // 2KB di RAM totali sul Pro Mini
    int ramOccupata = ramTotale - ramLibera;

    display.setCursor(68, 0);
    display.print("RAM:");
    display.setCursor(68, 12);
    display.print(ramOccupata);
    display.print("/");
    display.print(ramTotale);
    display.print("B");
    
    // --- LINEA ORIZZONTALE DI DIVISIONE GENERALE (A pixel Y = 24)
    display.drawFastHLine(0, 24, SCREEN_WIDTH, SSD1306_WHITE);

    // --- PARTE BASSA: Termometro
    display.setCursor(0, 32);
    display.print("Temp: ");
    display.print(temperaturaAttuale, 1); 
    display.print(" C");

    display.setCursor(0, 48);
    display.print("Umid: ");
    display.print(umiditaAttuale, 0);     
    display.print(" %");
    
    display.display(); 
    lastPos4x = pos4x; 
  }
  else if (paginaCorrente == 1) {
    // --------------------------------------------------------
    // PAGINA 2: SOLO GRAFICO A TUTTO SCHERMO
    // --------------------------------------------------------
    display.fillRect(0, 0, SCREEN_WIDTH, 64, SSD1306_BLACK); 

    display.drawFastVLine(14, 4, 54, SSD1306_WHITE);
    display.drawFastHLine(14, 58, 110, SSD1306_WHITE);

    if (puntiMemorizzati == 0) {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(30, 28);
      display.print("Campionamento...");
      display.display();
    } 
    else {
      float tempMin = 100.0;
      float tempMax = -100.0;
      for (int i = 0; i < puntiMemorizzati; i++) {
        if (storiciTemperatura[i] < tempMin) tempMin = storiciTemperatura[i];
        if (storiciTemperatura[i] > tempMax) tempMax = storiciTemperatura[i];
      }
      
      if (tempMax == tempMin) {
        tempMax += 1.0;
        tempMin -= 1.0;
      }

      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 4);   display.print((int)tempMax);
      display.setCursor(0, 48);  display.print((int)tempMin);

      float spazioX = 110.0 / (MAX_PUNTI - 1);
      int altezzaGraficoMax = 50; 

      int prevX = 0;
      int prevY = 0;

      for (int i = 0; i < puntiMemorizzati; i++) {
        int x_pixel = 14 + (int)(i * spazioX);
        float percentuale = (storiciTemperatura[i] - tempMin) / (tempMax - tempMin);
        int y_pixel = 57 - (int)(percentuale * altezzaGraficoMax);

        display.drawPixel(x_pixel, y_pixel, SSD1306_WHITE);

        if (i > 0) {
          display.drawLine(prevX, prevY, x_pixel, y_pixel, SSD1306_WHITE);
        }

        prevX = x_pixel;
        prevY = y_pixel;
      }
      display.display();
    }
  }

  delay(1); 
}