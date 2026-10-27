/*
 * KUMBH RAKSHAK - Kid Safety Band (ESP32, Bluetooth Low Energy)
 * -------------------------------------------------------------
 * Sends to the app every 2 seconds:   "x,y,battery,panic"
 *   x, y    = child's position in METRES from the meeting point (east, north)
 *   battery = 0-100
 *   panic   = 1 if the button was pressed, else 0
 * The app can send "RING" to make the band beep.
 *
 * Works in two modes:
 *   USE_GPS 0  -> DEMO mode (no GPS module). Position is simulated.
 *                 Type 'w' in the Serial Monitor to make the child "wander".
 *   USE_GPS 1  -> real position from a NEO-6M GPS module (needs TinyGPSPlus library)
 *
 * Wiring (ESP32 DevKit):
 *   Push button : GPIO 4  -> GND   (uses internal pull-up)
 *   Buzzer (+)  : GPIO 5  -> buzzer -> GND
 *   LED         : GPIO 2  (built-in LED on most boards)
 *   GPS NEO-6M  : GPS TX -> GPIO 16, GPS RX -> GPIO 17, VCC 3.3V, GND
 *   Battery     : optional, via 100k+100k voltage divider to GPIO 34
 *
 * Arduino IDE: Board = "ESP32 Dev Module". Library: TinyGPSPlus (only if USE_GPS 1).
 * NOTE: written for the standard ESP32 BLE library; test it on your board.
 */
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <math.h>

#define USE_GPS        0       // 1 = real GPS, 0 = demo simulation
#define USE_BATT_ADC   0       // 1 = read battery on GPIO 34, 0 = simulated drain
#define BAND_NAME      "KumbhBand-1000"

// Reference (meeting point). Change to your real meeting point coordinates.
const double BASE_LAT = 25.4358;
const double BASE_LON = 81.8463;

#define BUTTON_PIN 4
#define BUZZER_PIN 5
#define LED_PIN    2
#define BATT_PIN   34

// Nordic UART style UUIDs - the app must use the SAME values
#define SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define RX_UUID      "0000ffe1-0000-1000-8000-00805f9b34fb"// app -> band (write)
#define TX_UUID      "0000ffe1-0000-1000-8000-00805f9b34fb"// band -> app (notify)

#if USE_GPS
  #include <TinyGPSPlus.h>
  TinyGPSPlus gps;
#endif

BLEServer *server = nullptr;
BLECharacteristic *txChar = nullptr;
bool connected = false;
volatile bool ringRequested = false;
bool panic = false;
bool wander = false;
double simX = 20, simY = 10;       // demo position in metres
int battery = 100;
unsigned long lastSend = 0, lastBatt = 0, panicUntil = 0;

class ServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer *s) { connected = true; }
  void onDisconnect(BLEServer *s) {
    connected = false;
    BLEDevice::startAdvertising();        // allow the app to reconnect
  }
};

class RxCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) {
    String v = c->getValue().c_str();
    v.trim();
    if (v == "RING") ringRequested = true;
    if (v == "CLEAR") panic = false;
  }
};

void beep(int ms) { digitalWrite(BUZZER_PIN, HIGH); delay(ms); digitalWrite(BUZZER_PIN, LOW); }

int readBattery() {
#if USE_BATT_ADC
  float v = analogRead(BATT_PIN) / 4095.0 * 3.3 * 2.0;       // divider 1:2
  int pct = (int)((v - 3.2) / (4.2 - 3.2) * 100);            // 3.2V empty, 4.2V full
  return constrain(pct, 0, 100);
#else
  if (millis() - lastBatt > 30000 && battery > 5) { battery--; lastBatt = millis(); }
  return battery;
#endif
}

void getPosition(int &x, int &y) {
#if USE_GPS
  while (Serial2.available()) gps.encode(Serial2.read());
  if (gps.location.isValid()) {
    double dy = (gps.location.lat() - BASE_LAT) * 111320.0;
    double dx = (gps.location.lng() - BASE_LON) * 111320.0 * cos(BASE_LAT * M_PI / 180.0);
    x = (int)dx; y = (int)dy;
    return;
  }
#endif
  // DEMO: the child drifts east when wander mode is on
  if (wander) simX += 6;
  x = (int)simX; y = (int)simY;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
#if USE_GPS
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
#endif

  BLEDevice::init(BAND_NAME);
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCB());
  BLEService *svc = server->createService(SERVICE_UUID);

  txChar = svc->createCharacteristic(TX_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  txChar->addDescriptor(new BLE2902());

  BLECharacteristic *rxChar = svc->createCharacteristic(RX_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rxChar->setCallbacks(new RxCB());

  svc->start();
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  beep(150);
  Serial.println("Band ready. Name: " BAND_NAME);
  Serial.println("Type 'w' to toggle wandering (demo), 'r' to reset position.");
}

void loop() {
  // Panic button (active LOW). Press once = panic for 15 seconds.
  if (digitalRead(BUTTON_PIN) == LOW) {
    panic = true; panicUntil = millis() + 15000;
    beep(80);
  }
  if (panic && millis() > panicUntil) panic = false;

  // Demo controls from the Serial Monitor
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'w') { wander = !wander; Serial.println(wander ? "Wandering ON" : "Wandering OFF"); }
    if (c == 'r') { simX = 20; simY = 10; wander = false; Serial.println("Position reset"); }
  }

  // Ring request from the app
  if (ringRequested) {
    ringRequested = false;
    for (int i = 0; i < 6; i++) { beep(250); delay(200); }
  }

  // Send data every 2 seconds
  if (millis() - lastSend >= 2000) {
    lastSend = millis();
    int x, y;
    getPosition(x, y);
    int b = readBattery();
    String msg = String(x) + "," + String(y) + "," + String(b) + "," + String(panic ? 1 : 0);
    if (connected) {
      txChar->setValue(msg);            // under 20 bytes, fits one BLE packet
      txChar->notify();
    }
    digitalWrite(LED_PIN, connected ? HIGH : !digitalRead(LED_PIN));   // blinking = waiting for app
    Serial.println(msg);
  }
}
