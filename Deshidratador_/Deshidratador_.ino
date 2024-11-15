#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"
#include "HX711.h"

// Configuración de los sensores y pines
#define DHTPIN1 15
#define DHTPIN2 4
#define DHTPIN3 17
#define DHTTYPE DHT22

const int LOADCELL1_DOUT_PIN = 19;
const int LOADCELL1_SCK_PIN = 18;
const int LOADCELL2_DOUT_PIN = 23;
const int LOADCELL2_SCK_PIN = 22;

DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);
DHT dht3(DHTPIN3, DHTTYPE);

HX711 balanza1;
HX711 balanza2;

WiFiClient client;

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando...");

  // Iniciar sensores DHT y balanzas
  dht1.begin();
  dht2.begin();
  dht3.begin();
  balanza1.begin(LOADCELL1_DOUT_PIN, LOADCELL1_SCK_PIN);
  balanza2.begin(LOADCELL2_DOUT_PIN, LOADCELL2_SCK_PIN);

  WiFi.begin("SSID", "PASSWORD");  // Cambia por tus credenciales WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado a WiFi.");
}

void loop() {
  enviarDatos();
  delay(900000); // Esperar 15 minutos antes de la siguiente lectura
}

void enviarDatos() {
  float t1 = dht1.readTemperature();
  float h1 = dht1.readHumidity();
  float t2 = dht2.readTemperature();
  float h2 = dht2.readHumidity();
  float t3 = dht3.readTemperature();
  float h3 = dht3.readHumidity();
  float peso1 = balanza1.get_units(10);
  float peso2 = balanza2.get_units(10);

  if (isnan(t1) || isnan(h1) || isnan(t2) || isnan(h2) || isnan(t3) || isnan(h3)) {
    Serial.println("Error en la lectura de sensores.");
    return;
  }

  HTTPClient http;
  http.begin(client, "https://ipnproyecto.samayoaprojects.com.mx/deshidratadores/deshidratadormanzanas1/data");
  
  // Para enviar en formato JSON
  
  http.addHeader("Content-Type", "application/json");
  String jsonData = "{\"temperatura1\":" + String(t1) + ",\"humedad1\":" + String(h1) +
                    ",\"temperatura2\":" + String(t2) + ",\"humedad2\":" + String(h2) +
                    ",\"temperatura3\":" + String(t3) + ",\"humedad3\":" + String(h3) +
                    ",\"pesogral\":" + String(peso1) + ",\"pesolvl\":" + String(peso2) + "}";
  int httpResponseCode = http.POST(jsonData);

  // Comprobar la respuesta del servidor
  if (httpResponseCode > 0) {
    Serial.println("Código de respuesta: " + String(httpResponseCode));
    if (httpResponseCode == 200) {
      String response = http.getString();
      Serial.println("Respuesta del servidor: " + response);
    }
  } else {
    Serial.println("Error en la conexión: " + String(httpResponseCode));
  }

  http.end();
}