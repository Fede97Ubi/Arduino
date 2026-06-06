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

// --- MEMORIZZAZIONE DATI GRAFICO AUMENTATA A 50 PUNTI ---
const int MAX_PUNTI = 50; 
float storiciTemperatura[MAX_PUNTI];
int puntiMemorizzati = 0;

float temperaturaAttuale = 0.0;
float umiditaAttuale = 0.0;

// --- GESTIONE ZOOM INTERATTIVO BILANCIATO ---
int rangeGradiY = 10; 
long vecchiaPosizioneEncoderGrafico = 0;

// Tempistiche
unsigned long previousMillisDHT = 0;
const long intervalDHT = 2000; 

unsigned long previousMillisGrafico = 0;
const long intervalGrafico = 10000; // 10 secondi

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
    
    if (paginaCorrente == 1) {
      enc.write(rangeGradiY * 4); 
      vecchiaPosizioneEncoderGrafico = rangeGradiY;
    }
    
    display.clearDisplay(); 
    display.fillRect(0, 0, SCREEN_WIDTH, 64, SSD1306_BLACK);
    display.display();
  }

  // --- Lettura Encoder per la Pagina 1
  long pos4x = enc.read();
  long pos1x = pos4x / 4; 

  // --- Controllo Zoom in Pagina 2
  if (paginaCorrente == 1) {
    if (pos1x != vecchiaPosizioneEncoderGrafico) {
      if (pos1x < 1) { pos1x = 1; enc.write(4); }
      if (pos1x > 20) { pos1x = 20; enc.write(80); }
      
      rangeGradiY = pos1x;
      vecchiaPosizioneEncoderGrafico = pos1x;
    }
  }

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
    // PAGINA 1: INTERFACCIA PULITA
    display.fillRect(0, 0, SCREEN_WIDTH, 64, SSD1306_BLACK); 
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    display.setCursor(0, 0);
    display.print(F("4x: ")); display.print(pos4x);
    display.setCursor(0, 12);
    display.print(F("1x: ")); display.print(pos1x);
    
    display.drawFastVLine(64, 0, 22, SSD1306_WHITE);

    int ramLibera = getRamLibera();
    int ramTotale = 2048; 
    int ramOccupata = ramTotale - ramLibera;

    display.setCursor(68, 0);
    display.print(F("RAM:"));
    display.setCursor(68, 12);
    display.print(ramOccupata);
    display.print(F("/"));
    display.print(ramTotale);
    display.print(F("B"));
    
    display.drawFastHLine(0, 24, SCREEN_WIDTH, SSD1306_WHITE);

    display.setCursor(0, 32);
    display.print(F("Temp: "));
    display.print(temperaturaAttuale, 1); 
    display.print(F(" C"));

    display.setCursor(0, 48);
    display.print(F("Umid: "));
    display.print(umiditaAttuale, 0);     
    display.print(F(" %"));
    
    display.display(); 
    lastPos4x = pos4x; 
  }
  else if (paginaCorrente == 1) {
    // PAGINA 2: GRAFICO CON CENTRATURA AD INTERI SIMMETRICA
    display.fillRect(0, 0, SCREEN_WIDTH, 64, SSD1306_BLACK); 

    if (puntiMemorizzati == 0) {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(40, 28);
      display.print(F("Raccolta..."));
      display.display();
    } 
    else {
      float somma = 0.0;
      for (int i = 0; i < puntiMemorizzati; i++) {
        somma += storiciTemperatura[i];
      }
      
      float mediaDati = somma / (float)puntiMemorizzati;
      
      int centroIntero = (int)mediaDati;

      int metaSotto = rangeGradiY / 2;
      int metaSopra = rangeGradiY - metaSotto; 

      int tempMinIntera = centroIntero - metaSotto;
      int tempMaxIntera = centroIntero + metaSopra;

      float limiteY_Min = (float)tempMinIntera;
      float limiteY_Max = (float)tempMaxIntera;

      float spazioX = 102.0 / (MAX_PUNTI - 1);
      int altezzaGraficoMax = 61; 

      int prevX = 0;
      int prevY = 0;

      for (int i = 0; i < puntiMemorizzati; i++) {
        int x_pixel = 26 + (int)(i * spazioX);
        
        float percentuale = (storiciTemperatura[i] - limiteY_Min) / (limiteY_Max - limiteY_Min);
        
        if (percentuale > 1.0) percentuale = 1.0;
        if (percentuale < 0.0) percentuale = 0.0;

        int y_pixel = 61 - (int)(percentuale * altezzaGraficoMax);

        display.drawPixel(x_pixel, y_pixel, SSD1306_WHITE);

        if (i > 0) {
          display.drawLine(prevX, prevY, x_pixel, y_pixel, SSD1306_WHITE);
        }

        prevX = x_pixel;
        prevY = y_pixel;
      }

      display.fillRect(0, 0, 26, 64, SSD1306_BLACK); 

      display.drawFastVLine(26, 0, 64, SSD1306_WHITE); 
      display.drawFastHLine(26, 63, 102, SSD1306_WHITE); 

      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      
      display.setCursor(0, 0);   display.print(tempMaxIntera);  
      display.setCursor(0, 54);  display.print(tempMinIntera);  

      display.setCursor(102, 0);
      display.print(F("R:")); display.print(rangeGradiY);

      display.display();
    }
  }

  if (paginaCorrente == 0) {
    delay(33); 
  } else {
    delay(1);
  }
}