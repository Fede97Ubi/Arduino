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

// Stato del pulsante per logica di rilascio
bool lastButtonState = false; 

// --- GESTIONE PAGINE ---
int paginaCorrente = 0; 
const int TOTALE_PAGINE = 3; 

// --- MEMORIZZAZIONE DATI GRAFICO ---
const int MAX_PUNTI = 50; 
float storiciTemperatura[MAX_PUNTI];
int puntiMemorizzati = 0;

float temperaturaAttuale = 0.0;
float umiditaAttuale = 0.0;
float percepitaAttuale = 0.0; 

// --- GESTIONE ZOOM INTERATTIVO ---
int rangeGradiY = 10; 
long vecchiaPosizioneEncoderGrafico = 0;

// Tempistiche
unsigned long previousMillisDHT = 0;
const long intervalDHT = 2000; 

unsigned long previousMillisGrafico = 0;
const long intervalGrafico = 10000; 

// --- ARRAY BITMAP PER ANIMAZIONE ANIMALETTO (PROGMEM) ---
const uint8_t alieno_frame1[] PROGMEM = {
  0x03, 0xc0, 0x07, 0xe0, 0x0f, 0xf0, 0x1f, 0xf8, 0x1f, 0xf8, 0x0f, 0xf0, 0x03, 0xc0, 0x07, 0xe0,
  0x0f, 0xf0, 0x1f, 0xf8, 0x3f, 0xfc, 0x7f, 0xfe, 0x67, 0xe6, 0x43, 0xc2, 0x01, 0x80, 0x03, 0xc0
};

const uint8_t alieno_frame2[] PROGMEM = {
  0x03, 0xc0, 0x07, 0xe0, 0x0f, 0xf0, 0x1f, 0xf8, 0x1f, 0xf8, 0x0f, 0xf0, 0x03, 0xc0, 0x07, 0xe0,
  0x0f, 0xf0, 0x1f, 0xf8, 0x3f, 0xfc, 0x7f, 0xfe, 0x3f, 0xfc, 0x1c, 0x38, 0x08, 0x10, 0x18, 0x18
};

// Variabili per l'animazione dell'animaletto
unsigned long previousMillisAnimazione = 0;
int frameAttuale = 0;
int petX = 56; 
int petY = 24; 

int displayMin = 0;
int displayMax = 0;

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

void calcolaLimitiSmart(float* buffer, int nPunti, int rangeY, float lastVal) {
  int minOccupato = (int)floor(lastVal);
  int maxOccupato = (int)ceil(lastVal);

  for (int i = nPunti - 2; i >= 0; i--) {
      float val = buffer[i];
      int valMin = (int)floor(val);
      int valMax = (int)ceil(val);

      int newMin = min(minOccupato, valMin);
      int newMax = max(maxOccupato, valMax);

      if ((newMax - newMin) <= rangeY) {
          minOccupato = newMin;
          maxOccupato = newMax;
      } else {
          break;
      }
  }

  int spanAttuale = maxOccupato - minOccupato;
  int espansioneNecessaria = rangeY - spanAttuale;

  if (espansioneNecessaria > 0) {
      int metaEspansione = espansioneNecessaria / 2;
      minOccupato -= metaEspansione;
      maxOccupato = minOccupato + rangeY;
  }

  displayMin = minOccupato;
  displayMax = maxOccupato;
}

void setup() {
  Serial.begin(9600);
  delay(1000); 

  // LOG DI AVVIO SULLA SERIALE
  Serial.println(F("====================================="));
  Serial.println(F("[SISTEMA] Arduino avviato correttamente!"));
  Serial.print(F("[INFO] Pagine totali configurate: ")); Serial.println(TOTALE_PAGINE);
  Serial.println(F("====================================="));

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("[ERRORE] Schermo OLED non trovato!"));
    while (true);
  }

  dht.begin();

  for (int i = 0; i < MAX_PUNTI; i++) {
    storiciTemperatura[i] = 0.0;
  }

  display.clearDisplay();
  display.display();

  pinMode(PIN_SW, INPUT_PULLUP);
  
  // Sincronizza lo stato iniziale del pulsante
  lastButtonState = !digitalRead(PIN_SW);
}

void loop() {
  unsigned long currentMillis = millis();

  // --- Gestione cambio pagina col Pulsante (Migliorata con logica di rilascio) ---
  bool currentButtonState = !digitalRead(PIN_SW); // true se premuto
  
  if (currentButtonState && !lastButtonState) {
    // Il pulsante è appena stato premuto (fronte di salita)
    if (currentMillis - lastButtonPress > debounceDelay) {
      lastButtonPress = currentMillis;
      
      int paginaPrecedente = paginaCorrente;
      paginaCorrente = (paginaCorrente + 1) % TOTALE_PAGINE;
      
      // LOG SERIALE DEL CLICK
      Serial.print(F("[INPUT] Pulsante premuto. Transizione: Pagina "));
      Serial.print(paginaPrecedente);
      Serial.print(F(" -> Pagina "));
      Serial.println(paginaCorrente);
      
      if (paginaCorrente == 1) {
        enc.write(rangeGradiY * 4); 
        vecchiaPosizioneEncoderGrafico = rangeGradiY;
      }
      
      display.clearDisplay(); 
      display.display();
    }
  }
  lastButtonState = currentButtonState; // Aggiorna lo stato per il prossimo ciclo

  // --- Lettura Encoder ---
  long pos4x = enc.read();
  long pos1x = pos4x / 4; 

  // --- Controllo Zoom (Attivo SOLO in Pagina 1) ---
  if (paginaCorrente == 1) {
    if (pos1x != vecchiaPosizioneEncoderGrafico) {
      if (pos1x < 1) { pos1x = 1; enc.write(4); }
      if (pos1x > 20) { pos1x = 20; enc.write(80); }
      
      rangeGradiY = pos1x;
      vecchiaPosizioneEncoderGrafico = pos1x;
      Serial.print(F("[GRAFICO] Nuovo range zoom impostato: ")); Serial.println(rangeGradiY);
    }
  }

  // --- Gestione Lettura Sensore (Ogni 2 secondi) ---
  if (currentMillis - previousMillisDHT >= intervalDHT) {
    previousMillisDHT = currentMillis;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      temperaturaAttuale = t;
      umiditaAttuale = h;
      percepitaAttuale = dht.computeHeatIndex(t, h, false);
    }
  }

  // --- Gestione Salvataggio Storico (Ogni 10 secondi) ---
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
    // PAGINA 1: INTERFACCIA DATI
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

    display.setCursor(0, 44);
    display.print(F("Umid: "));
    display.print(umiditaAttuale, 0);     
    display.print(F(" %"));

    display.setCursor(0, 56);
    display.print(F("Perc: "));
    display.print(percepitaAttuale, 1);     
    display.print(F(" C"));

    display.drawFastVLine(78, 28, 36, SSD1306_WHITE);

    unsigned long secondiUp = currentMillis / 1000;
    display.setCursor(84, 36);
    display.print(F("UPTIME"));
    display.setCursor(84, 48);
    display.print(secondiUp);
    display.print(F("s"));
    
    display.display(); 
    lastPos4x = pos4x; 
  }
  else if (paginaCorrente == 1) {
    // PAGINA 2: GRAFICO
    display.fillRect(0, 0, SCREEN_WIDTH, 64, SSD1306_BLACK); 

    if (puntiMemorizzati == 0) {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(40, 28);
      display.print(F("Raccolta..."));
      display.display();
    } 
    else {
      calcolaLimitiSmart(storiciTemperatura, puntiMemorizzati, rangeGradiY, storiciTemperatura[pUNTI_mEMORIZZATI - 1]);
      
      float limiteY_Min = (float)displayMin;
      float limiteY_Max = (float)displayMax;

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
      
      display.setCursor(0, 0);   display.print(displayMax);  
      display.setCursor(0, 54);  display.print(displayMin);

      display.setCursor(102, 0);
      display.print(F("R:")); display.print(rangeGradiY);

      display.display();
    }
  } 
  else if (paginaCorrente == 2) {
    // PAGINA 3: ARDU-PET ANIMATO
    display.fillRect(0, 0, SCREEN_WIDTH, 64, SSD1306_BLACK);
    
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print(F("PAG 3: ARDU-PET"));
    display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

    // Animazione dell'animaletto
    if (currentMillis - previousMillisAnimazione >= 250) {
      previousMillisAnimazione = currentMillis;
      frameAttuale = (frameAttuale + 1) % 2; 
      
      petX += random(-3, 4); 
      if (petX < 5) petX = 5;
      if (petX > 105) petX = 105;
    }

    if (frameAttuale == 0) {
      display.drawBitmap(petX, petY, alieno_frame1, 16, 16, SSD1306_WHITE);
    } else {
      display.drawBitmap(petX, petY, alieno_frame2, 16, 16, SSD1306_WHITE);
    }

    // Terreno fisso
    display.drawFastHLine(0, 60, SCREEN_WIDTH, SSD1306_WHITE);
    for(int i = 0; i < 128; i += 8) {
      display.drawLine(i, 60, i + 2, 57, SSD1306_WHITE);
    }

    display.display();
  }

  delay(10); 
}