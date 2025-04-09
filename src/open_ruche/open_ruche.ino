// #define DEBUG_SERIAL


#include "HX711.h"
#include <DHT11.h>
#include <DHT22.h>
#include <OneWire.h>
#include "Wire.h"
#include <MKRWAN.h>
#include <Arduino_JSON.h>

LoRaModem modem;
HX711 scale;

#define SUICIDE_PIN 5

//  adjust pins if needed
uint8_t BALANCE_dataPin = 8;
uint8_t BALANCE_clockPin = 7;
#define SCALE_CALIB 29.299475
#define OFFSET_CALIB 87021

#define BATTERIE_PIN A0
#define BATTERIE_MIN 3.3
#define BATTERIE_MAX 4.2
#define BATTERIE_COEF 1.5
#define BATTERIE_REF 4.92

#define DHT11_PIN 2
#define DHT22_PIN 3

#define ONE_WIRE_BUS 10  // Pin de connexion du capteur
#define LUX_ADRESSE 0x23  // I2C address 0x23
int oneWireLastGoodValue = -1000;


int refresh_rate = 2000;

DHT22 dht11(DHT11_PIN);
DHT22 dht22(DHT22_PIN);

OneWire ds(ONE_WIRE_BUS);

float mesure_poids() {
  float poids = scale.get_units(5)/1000;
  return poids;
}

float mesure_batterie() {
  return min(100., max(0., ((analogRead(A0) * BATTERIE_REF / 1023.) - BATTERIE_MIN) / (BATTERIE_MAX - BATTERIE_MIN) *100 ));
}

void mesure_DHT11(float& temp, float& hum) {
  temp = dht11.getTemperature();
  hum = dht11.getHumidity();
  // dht11.readTemperatureHumidity(temp, hum);
}

void mesure_DHT22(float& temp, float& hum) {
  temp = dht22.getTemperature();
  hum = dht22.getHumidity();
}

float mesure_temp_onewire() {
  byte data[2];
  byte addr[8];

  if (!ds.search(addr)) {
    ds.reset_search();
    return oneWireLastGoodValue;  // Valeur d'erreur si aucun capteur n'est détecté
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);  // Démarrer la conversion

  delay(750);  // Attendre la fin de la conversion (750 ms pour 12 bits)

  ds.reset();
  ds.select(addr);
  ds.write(0xBE);  // Lire les données du scratchpad

  data[0] = ds.read();
  data[1] = ds.read();

  int16_t raw = (data[1] << 8) | data[0];  // Fusionner les octets
  float temp = (float)raw / 16.0;  // Convertir en °C
  if(temp != -1000.0) {
    oneWireLastGoodValue = temp;
  }
  else {
    temp = oneWireLastGoodValue;
  }

  return temp;
}

float mesure_LUX() {
  uint8_t buf[2] = { 0 };  // Taille du tableau correcte
  uint16_t data;

  // Lire les données du capteur (reg 0x10)
  readReg(0x10, buf, 2);
  data = buf[0] << 8 | buf[1];

  // Convertir la donnée brute en lux
  return (float)data / 1.2;
}

uint8_t readReg(uint8_t reg, const void* pBuf, size_t size) {
  if (pBuf == NULL) {
#ifdef DEBUG_SERIAL
    Serial.println("pBuf ERROR!! : null pointer");
#endif
    return 0;
  }

  uint8_t* _pBuf = (uint8_t*)pBuf;
  Wire.beginTransmission(LUX_ADRESSE);
  Wire.write(&reg, 1);
  if (Wire.endTransmission() != 0) {
    return 0;
  }
  delay(20);  // Temps d'attente pour permettre au capteur de répondre
  Wire.requestFrom(LUX_ADRESSE, (uint8_t)size);
  for (uint16_t i = 0; i < size; i++) {
    _pBuf[i] = Wire.read();
  }
  return size;
}

void killAfter(int millis){
  delay(millis);
  digitalWrite(SUICIDE_PIN, 1);
}

void setup() {
  pinMode(SUICIDE_PIN, OUTPUT);
  digitalWrite(SUICIDE_PIN, 0);

#ifdef DEBUG_SERIAL
  Serial.begin(9600);
  Serial.println("Hello");
#endif
  tone(4, 800, 1000);
  delay(1500);

  // Init balance
  scale.begin(BALANCE_dataPin, BALANCE_clockPin);
  scale.set_offset(OFFSET_CALIB);
  scale.set_scale(SCALE_CALIB);
  // scale.tare();

  // Init I2C
  Wire.begin();

  // Initialisation LoRa
  if (!modem.begin(EU868)) {
#ifdef DEBUG_SERIAL
    Serial.println("Failed to start module");
#endif
    killAfter(10000);
  };
#ifdef DEBUG_SERIAL
  Serial.print("Your module version is: ");
  Serial.println(modem.version());
  Serial.print("Your device EUI is: ");
  Serial.println(modem.deviceEUI());
#endif

  int connected = modem.joinOTAA("0000000000000000", "C9BC6AFA5457345D840FB6F33A9873E2");
  if (!connected) {
#ifdef DEBUG_SERIAL
    Serial.println("Connection failed, retrying...");
#endif
    killAfter(20000);
  }

  modem.minPollInterval(120);  // Intervalle minimum pour les envois


  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 1);
  delay(500);
  digitalWrite(LED_BUILTIN, 0);
  delay(500);
  digitalWrite(LED_BUILTIN, 1);
  delay(100);
  digitalWrite(LED_BUILTIN, 0);
  delay(100);
  digitalWrite(LED_BUILTIN, 1);
  delay(100);
  digitalWrite(LED_BUILTIN, 0);
}

bool sendLoRaData(String data) {
  bool status = false;
  modem.beginPacket();
  modem.print(data);
  int err = modem.endPacket(true);
  if (err > 0) {
    digitalWrite(LED_BUILTIN, 1);
#ifdef DEBUG_SERIAL
    Serial.println("Data sent successfully!");
#endif
    delay(200);
    digitalWrite(LED_BUILTIN, 0);
    delay(500);
    digitalWrite(LED_BUILTIN, 1);
    delay(200);
    digitalWrite(LED_BUILTIN, 0);
    status = true;
  } 
#ifdef DEBUG_SERIAL
  else {
    Serial.println("Error sending data");
  }
#endif
  
  delay(1000);
#ifdef DEBUG_SERIAL
  if (!modem.available()) {
    Serial.println("No downlink message received at this time.");

  }
#endif
  char rcv[64];
  int i = 0;
  while (modem.available()) {
    rcv[i++] = (char)modem.read();
  }
#ifdef DEBUG_SERIAL
  Serial.print("Received: ");
  for (unsigned int j = 0; j < i; j++) {
    Serial.print(rcv[j] >> 4, HEX);
    Serial.print(rcv[j] & 0xF, HEX);
    Serial.print(" ");
  }
  Serial.println();
#endif
  return status;
}

void loop() {
  // Mesure des différents capteurs
  float pourc_bat = mesure_batterie();
  float poids = mesure_poids();
  float temperature, humidity;
  mesure_DHT11(temperature, humidity);
  float ftemperature, fhumidity;
  mesure_DHT22(ftemperature, fhumidity);
  float tempOW = mesure_temp_onewire()-1.;
  float lux = mesure_LUX();

  // Créer la chaîne de valeurs séparées par des virgules
  String data = String(poids, 3) + ",";  // Format avec 3 décimales
  data += String(pourc_bat, 1) + ",";   // Format avec 1 décimale
  data += String(temperature, 2) + ",";    // Entier
  data += String(humidity, 1) + ",";       // Entier
  data += String(ftemperature, 2) + ","; // Format avec 3 décimales
  data += String(fhumidity, 1) + ",";   // Format avec 1 décimale
  data += String(tempOW, 3) + ",";      // Format avec 3 décimales
  data += String(lux, 3);               // Format avec 3 décimales


#ifdef DEBUG_SERIAL
  Serial.println(data);
  Serial.print("len: ");
  Serial.println(data.length());
#endif

  // Serial.println(data);
  if(sendLoRaData(data)) {
    tone(4, 2000, 500);
    delay(500);
  }

  delay(20000);  // Attendre 20 secondes avant d'envoyer les données à nouveau
  digitalWrite(SUICIDE_PIN, 1);
}
