// UCA - Programacion de Artefactos - Laboratorio 6
// Prueba de sensores fisicos + MQTT a Mosquitto y dashboard React.
// DHT11: DATA GPIO4, VCC 3V3, GND GND.
// LDR: 3V3 -> LDR -> union GPIO32 -> resistencia ~5k -> GND.
// No alimentar antes de revisar las conexiones y confirmar el valor de la resistencia.

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// Wi-Fi compartido desde el iPhone. El nombre debe coincidir EXACTAMENTE.
const char* WIFI_SSID = "Alejandro’s iPhone";
const char* WIFI_PASSWORD = "12345678";

// IPv4 de la laptop mediante iPhone USB tethering (Ethernet 2).
const char* MQTT_SERVER = "172.20.10.2";
const uint16_t MQTT_PORT = 1883;

const uint8_t PIN_DHT = 4;
const uint8_t PIN_LDR = 32;
#define DHT_TIPO DHT11

const char* TOPIC_TEMP = "clima/esp32/temperatura";
const char* TOPIC_HUM = "clima/esp32/humedad";
const char* TOPIC_LUZ = "clima/esp32/luz";

const unsigned long INTERVALO_LECTURA = 2000;
const unsigned long INTERVALO_RECONEXION = 5000;

DHT dht(PIN_DHT, DHT_TIPO);
WiFiClient tcp;
PubSubClient mqtt(tcp);

unsigned long ultimaLectura = 0;
unsigned long ultimoIntentoWiFi = 0;
unsigned long ultimoIntentoMQTT = 0;
unsigned int contador = 0;
bool wifiEstabaConectado = false;

void gestionarWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiEstabaConectado) {
      wifiEstabaConectado = true;
      Serial.print("WiFi conectado. IP del ESP32: ");
      Serial.println(WiFi.localIP());
      Serial.print("Broker Mosquitto: ");
      Serial.print(MQTT_SERVER);
      Serial.print(":");
      Serial.println(MQTT_PORT);
    }
    return;
  }

  wifiEstabaConectado = false;
  const unsigned long ahora = millis();
  if (ultimoIntentoWiFi != 0 && (ahora - ultimoIntentoWiFi < INTERVALO_RECONEXION)) return;
  ultimoIntentoWiFi = ahora;
  Serial.print("Conectando al WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void gestionarMQTT() {
  if (WiFi.status() != WL_CONNECTED || mqtt.connected()) return;
  const unsigned long ahora = millis();
  if (ultimoIntentoMQTT != 0 && (ahora - ultimoIntentoMQTT < INTERVALO_RECONEXION)) return;
  ultimoIntentoMQTT = ahora;

  String clientId = "esp32-lab6-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  Serial.print("Conectando a Mosquitto en ");
  Serial.println(MQTT_SERVER);
  if (mqtt.connect(clientId.c_str())) {
    Serial.println("MQTT CONECTADO");
  } else {
    Serial.print("MQTT fallo; state = ");
    Serial.println(mqtt.state());
  }
}

void tomarMedicion() {
  const float temperatura = dht.readTemperature();
  const float humedad = dht.readHumidity();
  const int adc = analogRead(PIN_LDR);
  // LDR conectada entre 3V3 y GPIO32: mas luz => mayor ADC.
  const int luz = constrain(map(adc, 0, 4095, 0, 100), 0, 100);

  Serial.println();
  Serial.print("Lectura #");
  Serial.println(++contador);
  Serial.print("LDR ADC: ");
  Serial.print(adc);
  Serial.print(" | Iluminacion estimada: ");
  Serial.print(luz);
  Serial.println(" %");

  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("DHT11: ERROR DE LECTURA. Revisar GPIO4, 3V3 y GND.");
    Serial.println("---------------------------------------");
    return;
  }

  Serial.print("Temperatura: ");
  Serial.print(temperatura, 1);
  Serial.println(" C");
  Serial.print("Humedad: ");
  Serial.print(humedad, 1);
  Serial.println(" %");

  if (mqtt.connected()) {
    const String t = String(temperatura, 1);
    const String h = String(humedad, 1);
    const String l = String(luz);
    const bool okT = mqtt.publish(TOPIC_TEMP, t.c_str(), true);
    const bool okH = mqtt.publish(TOPIC_HUM, h.c_str(), true);
    const bool okL = mqtt.publish(TOPIC_LUZ, l.c_str(), true);
    Serial.println(okT && okH && okL ? "MQTT: 3 mediciones publicadas" : "MQTT: fallo al publicar");
  } else {
    Serial.println("MQTT sin conexion; las mediciones locales continuan.");
  }
  Serial.println("---------------------------------------");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== UCA LAB6 - ESP32 + DHT11 + LDR + MQTT ===");
  dht.begin();
  pinMode(PIN_LDR, INPUT);
  analogReadResolution(12);
  WiFi.mode(WIFI_STA);
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setSocketTimeout(2);
  gestionarWiFi();
}

void loop() {
  gestionarWiFi();
  gestionarMQTT();
  if (mqtt.connected()) mqtt.loop();
  const unsigned long ahora = millis();
  if (ahora - ultimaLectura >= INTERVALO_LECTURA) {
    ultimaLectura = ahora;
    tomarMedicion();
  }
  delay(10);
}