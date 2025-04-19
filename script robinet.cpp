// Bibliothèques nécessaires pour l’écran LCD, la communication SPI et le lecteur RFID
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>

// Définition des broches utilisées
#define RST_PIN 9
#define SS_PIN 10
#define TRIGGER_PIN 7
#define ECHO_PIN 6
#define RELAY_PIN 5
#define PULSE_SENSOR 2

// Variables RFID
byte storedCard[4];
bool isMasterSet = false;

// Variables capteur de débit
volatile int pulseCount = 0;
float totalUsed = 0.0;
float instantConsumption = 0.0;
unsigned long lastPulseTime = 0;

// Débit amélioré
static unsigned long oldTime = 0;
float calibrationFactor = 90.0;

// Variables relais
unsigned long relayActivationTime = 0;
bool relayActive = false;

// LCD et RFID
LiquidCrystal_I2C lcd(0x20, 16, 2);
MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  lcd.init(); lcd.backlight();
  SPI.begin(); mfrc522.PCD_Init();

  lcd.setCursor(0, 0); lcd.print("Scanner votre");
  lcd.setCursor(0, 1); lcd.print("carte RFID");

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(PULSE_SENSOR, INPUT);

  attachInterrupt(digitalPinToInterrupt(PULSE_SENSOR), pulseCounter, FALLING);
  digitalWrite(RELAY_PIN, LOW);
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  byte currentCard[4];
  for (uint8_t i = 0; i < 4; i++) currentCard[i] = mfrc522.uid.uidByte[i];

  if (!isMasterSet) {
    for (uint8_t i = 0; i < 4; i++) storedCard[i] = currentCard[i];
    isMasterSet = true;
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("Carte");
    lcd.setCursor(0, 1); lcd.print("enrengistree");
    delay(2000);
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("Scannez votre");
    lcd.setCursor(0, 1); lcd.print("carte");
    return;
  }

  bool accessGranted = true;
  for (uint8_t i = 0; i < 4; i++) {
    if (currentCard[i] != storedCard[i]) {
      accessGranted = false;
      break;
    }
  }

  lcd.clear();
  if (accessGranted) {
    lcd.print("Acces autorise");
    delay(2000);
    lcd.clear(); lcd.print("Placez la main.");
    for (int i = 10; i > 0; i--) {
      lcd.setCursor(0, 1); lcd.print("Attente: ");
      lcd.print(i); lcd.print(" sec  ");
      delay(1000);
      if (detectMotion()) break;
    }

    if (!detectMotion()) {
      lcd.clear(); lcd.setCursor(0, 0); lcd.print("Scannez votre");
      lcd.setCursor(0, 1); lcd.print("carte");
      return;
    }

    lcd.clear();
    digitalWrite(RELAY_PIN, HIGH);
    relayActivationTime = millis();
    relayActive = true;

    while (relayActive) {
      measureFlowRate();
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Debit: ");
      lcd.print(instantConsumption * 1000.0, 0); // mL/s
      lcd.print(" mL/s");

      if (millis() - relayActivationTime > 5000) {
        digitalWrite(RELAY_PIN, LOW);
        relayActive = false;

        lcd.clear();
        lcd.setCursor(0, 1); lcd.print("Total: ");
        lcd.print(totalUsed * 1000.0, 0); lcd.print(" mL");
        delay(5000);

        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("Scannez votre");
        lcd.setCursor(0, 1); lcd.print("carte");
      }

      delay(1000);
    }
  } else {
    lcd.print("Acces refuse");
    delay(2000);
    lcd.setCursor(0, 0); lcd.print("Scannez votre");
    lcd.setCursor(0, 1); lcd.print("carte");
  }
}

bool detectMotion() {
  long duration, distance;
  digitalWrite(TRIGGER_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH);
  distance = (duration / 2) / 29.1;
  return (distance <= 5 && distance > 0);
}

void measureFlowRate() {
  if ((millis() - oldTime) > 1000) {
    detachInterrupt(digitalPinToInterrupt(PULSE_SENSOR));
    float flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
    oldTime = millis();

    float flowMilliLitres = (flowRate / 60.0) * 1000.0;
    instantConsumption = flowMilliLitres / 1000.0; // en L/s
    totalUsed += flowMilliLitres / 1000.0;

    pulseCount = 0;
    attachInterrupt(digitalPinToInterrupt(PULSE_SENSOR), pulseCounter, FALLING);
  }
}

void pulseCounter() {
  pulseCount++;
}
