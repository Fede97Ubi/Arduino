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
DHT dht_real(DHTPIN, DHTTYPE); 

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
const int MAX_PUNTI = 40; 
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

// --- STATI ESPRESSIONI OCCHI ---
#define OCCHI_NORMALI 0
#define OCCHI_BLINK   1
#define OCCHI_SINISTRA 2
#define OCCHI_DESTRA   3
#define OCCHI_FELICE   4
#define OCCHI_ARRABBIATO 5

unsigned long tempoUltimoStatoOcchi = 0;
int statoOcchiAttuale = OCCHI_NORMALI;
unsigned long durataStatoOcchi = 2000; 

int displayMin = 0;
int displayMax = 0;

// --- VARIABILI MOTORE ANIMAZIONE SCATTANTE ---
int8_t cX = 0;       // Posizione X Corrente
int8_t cY = 0;       // Posizione Y Corrente
int8_t cW = 36;      // Larghezza Corrente
int8_t cH = 38;      // Altezza Corrente
int8_t cMask = 0;    // Altezza Maschera Felicità
int8_t cAngry = 0;   // Inclinazione Sopracciglia Rabbia

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
  // SERIALE RIMOSSO: Libera circa 160 Byte di RAM! Fine dei pixel fantasma.
  delay(500); 

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true); // Se lo schermo non si avvia, blocca tutto.
  }

  dht_real.begin();

  for (int i = 0; i < MAX_PUNTI; i++) {
    storiciTemperatura[i] = 0.0;
  }

  display.clearDisplay();
  display.display();

  pinMode(PIN_SW, INPUT_PULLUP);
  lastButtonState = !digitalRead(PIN_SW);
}

void loop() {
  unsigned long currentMillis = millis();

  // --- Gestione cambio pagina col Pulsante ---
  bool currentButtonState = !digitalRead(PIN_SW); 
  
  if (currentButtonState && !lastButtonState) {
    if (currentMillis - lastButtonPress > debounceDelay) {
      lastButtonPress = currentMillis;
      
      paginaCorrente = (paginaCorrente + 1) % TOTALE_PAGINE;
      
      if (paginaCorrente == 1) {
        enc.write(rangeGradiY * 4); 
        vecchiaPosizioneEncoderGrafico = rangeGradiY;
      }
      
      display.clearDisplay(); 
      display.display();
    }
  }
  lastButtonState = currentButtonState; 

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
    }
  }

  // --- Gestione Lettura Sensore ---
  if (currentMillis - previousMillisDHT >= intervalDHT) {
    previousMillisDHT = currentMillis;
    float t = dht_real.readTemperature();
    float h = dht_real.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      temperaturaAttuale = t;
      umiditaAttuale = h;
      percepitaAttuale = dht_real.computeHeatIndex(t, h, false);
    }
  }

  // --- Salvataggio Grafico ---
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
    display.fillRect(0, 0, SCREEN_WIDTH, 64, SSD1306_BLACK); 
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    display.setCursor(0, 0);
    display.print(F("4x: ")); display.print(pos4x);
    display.setCursor(0, 12);
    display.print(F("1x: ")); display.print(pos1x);
    
    display.drawFastVLine(64, 0, 22, SSD1306_WHITE);

    int ramOccupata = 2048 - getRamLibera();

    display.setCursor(68, 0);
    display.print(F("RAM:"));
    display.setCursor(68, 12);
    display.print(ramOccupata);
    display.print(F("/2048B"));
    
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

    display.setCursor(84, 36);
    display.print(F("UPTIME"));
    display.setCursor(84, 48);
    display.print(currentMillis / 1000);
    display.print(F("s"));
    
    display.display(); 
    lastPos4x = pos4x; 
  }
  else if (paginaCorrente == 1) {
    display.fillRect(0, 0, SCREEN_WIDTH, 64, SSD1306_BLACK); 

    if (puntiMemorizzati == 0) {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(40, 28);
      display.print(F("Raccolta..."));
      display.display();
    } 
    else {
      calcolaLimitiSmart(storiciTemperatura, puntiMemorizzati, rangeGradiY, storiciTemperatura[puntiMemorizzati - 1]);
      
      float limiteY_Min = (float)displayMin;
      float limiteY_Max = (float)displayMax;
      float spazioX = 102.0 / (MAX_PUNTI - 1);
      int altezzaGraficoMax = 61; 

      int prevX = 0, prevY = 0;

      for (int i = 0; i < puntiMemorizzati; i++) {
        int x_pixel = 26 + (int)(i * spazioX);
        float percentuale = (storiciTemperatura[i] - limiteY_Min) / (limiteY_Max - limiteY_Min);
        
        if (percentuale > 1.0) percentuale = 1.0;
        if (percentuale < 0.0) percentuale = 0.0;

        int y_pixel = 61 - (int)(percentuale * altezzaGraficoMax);
        display.drawPixel(x_pixel, y_pixel, SSD1306_WHITE);

        if (i > 0) display.drawLine(prevX, prevY, x_pixel, y_pixel, SSD1306_WHITE);
        prevX = x_pixel; prevY = y_pixel;
      }

      display.fillRect(0, 0, 26, 64, SSD1306_BLACK); 
      display.drawFastVLine(26, 0, 64, SSD1306_WHITE); 
      display.drawFastHLine(26, 63, 102, SSD1306_WHITE); 

      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);   display.print(displayMax);  
      display.setCursor(0, 54);  display.print(displayMin);
      display.setCursor(102, 0); display.print(F("R:")); display.print(rangeGradiY);

      display.display();
    }
  } 
  else if (paginaCorrente == 2) {
    display.fillRect(0, 0, SCREEN_WIDTH, 64, SSD1306_BLACK);
    
    // --- Scelta Casuale Espressione ---
    if (currentMillis - tempoUltimoStatoOcchi > durataStatoOcchi) {
      tempoUltimoStatoOcchi = currentMillis;
      
      int dadi = random(0, 100);
      if (dadi < 45) {
        statoOcchiAttuale = OCCHI_NORMALI;
        durataStatoOcchi = random(2000, 4000);
      } else if (dadi < 60) {
        statoOcchiAttuale = OCCHI_BLINK;
        durataStatoOcchi = 140; 
      } else if (dadi < 73) {
        statoOcchiAttuale = OCCHI_SINISTRA;
        durataStatoOcchi = random(1500, 2500);
      } else if (dadi < 86) {
        statoOcchiAttuale = OCCHI_DESTRA;
        durataStatoOcchi = random(1500, 2500);
      } else if (dadi < 93) {
        statoOcchiAttuale = OCCHI_FELICE;
        durataStatoOcchi = random(2000, 3500);
      } else {
        statoOcchiAttuale = OCCHI_ARRABBIATO;
        durataStatoOcchi = random(2000, 3500);
      }
    }

    // Parametri Obiettivo da raggiungere
    int8_t tX = 0, tY = 0, tW = 36, tH = 38, tMask = 0, tAngry = 0;

    // Sguardo laterale AUMENTATO (-18 e +18 invece di 8)
    if (statoOcchiAttuale == OCCHI_SINISTRA)        { tX = -18; }
    else if (statoOcchiAttuale == OCCHI_DESTRA)     { tX = 18; }
    else if (statoOcchiAttuale == OCCHI_BLINK)      { tH = 2; }
    else if (statoOcchiAttuale == OCCHI_FELICE)     { tMask = 18; }
    // Rabbia introdotta: scendono le sopracciglia (14px) e l'occhio guarda giù (+3px)
    else if (statoOcchiAttuale == OCCHI_ARRABBIATO) { tAngry = 16; tY = 3; }

    // --- MOTORE INTERPOLAZIONE: MOLTO PIÙ SCATTANTE E VELOCE ---
    
    // Spostamento X (Destra / Sinistra) -> Passo da 6
    if (cX < tX) { cX += 6; if (cX > tX) cX = tX; }
    else if (cX > tX) { cX -= 6; if (cX < tX) cX = tX; }

    // Spostamento Y -> Passo da 3
    if (cY < tY) { cY += 3; if (cY > tY) cY = tY; }
    else if (cY > tY) { cY -= 3; if (cY < tY) cY = tY; }

    // Maschera del Sorriso -> Passo raddoppiato (8), sale subito
    if (cMask < tMask) { cMask += 8; if (cMask > tMask) cMask = tMask; }
    else if (cMask > tMask) { cMask -= 8; if (cMask < tMask) cMask = tMask; }

    // Maschera della Rabbia (Sopracciglia) -> Calano subito
    if (cAngry < tAngry) { cAngry += 8; if (cAngry > tAngry) cAngry = tAngry; }
    else if (cAngry > tAngry) { cAngry -= 8; if (cAngry < tAngry) cAngry = tAngry; }

    // Battito ciglia -> Istantaneo
    int8_t diffH = tH - cH;
    if (diffH != 0) {
      if (abs(diffH) > 10) cH += (diffH > 0) ? 12 : -12;
      else                 cH += (diffH > 0) ? 3 : -3;
    }

    // --- DISEGNO ---
    int posY_Occhi = 32 - (cH / 2) + cY;
    int posX_Sinistro = 20 + cX;
    int posX_Destro = 72 + cX;
    int raggioAngoli = (cH < 20) ? cH / 2 : 10;

    // Occhi Base
    display.fillRoundRect(posX_Sinistro, posY_Occhi, cW, cH, raggioAngoli, SSD1306_WHITE);
    display.fillRoundRect(posX_Destro, posY_Occhi, cW, cH, raggioAngoli, SSD1306_WHITE);

    // Sorriso (Taglio dal basso verso l'alto)
    if (cMask > 0) {
      display.fillRect(posX_Sinistro, posY_Occhi + cH - cMask, cW, cMask, SSD1306_BLACK);
      display.fillRect(posX_Destro, posY_Occhi + cH - cMask, cW, cMask, SSD1306_BLACK);
    } 

    // Rabbia Animata (I triangoli scendono con la variabile cAngry)
    if (cAngry > 0) {
      display.fillTriangle(posX_Sinistro + cW - 16, posY_Occhi, 
                           posX_Sinistro + cW, posY_Occhi, 
                           posX_Sinistro + cW, posY_Occhi + cAngry, SSD1306_BLACK);
                           
      display.fillTriangle(posX_Destro, posY_Occhi, 
                           posX_Destro + 16, posY_Occhi, 
                           posX_Destro, posY_Occhi + cAngry, SSD1306_BLACK);
    }

    display.display();
  }

  delay(10); 
}