#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <BH1750.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

#define DHTPIN 4                // Pin conectado al DHT22
#define DHTTYPE DHT22           // Definir tipo de sensor como DHT22

DHT dht(DHTPIN, DHTTYPE);     // Crear objeto DHT

LiquidCrystal_I2C lcd(0x27, 16, 2); // Dirección I2C del LCD

// Definir credenciales de la red WiFi
const char* ssid = "TilinesTec";
const char* password = "ConectatePa123";

// URL del servidor para enviar datos
const char* serverUrl = "http://ipnproyecto.samayoaprojects.com.mx/germinadores/germinadorlab/data";

// Variables de lectura de sensores
float t;
float h;
float lux;

// Crear objeto para el sensor BH1750
BH1750 lightMeter;

// Variables para el control de la pantalla y temporizadores
unsigned long tiempoAnteriorPantalla = 0;
unsigned long tiempoAnteriorEnvio = 0;
unsigned long tiempoAnteriorRutina = 0;

const long intervaloPantalla = 2000;  // Intervalo de 2 segundos para actualización del LCD
const long intervaloEnvio = 60000;    // Intervalo de 1 minuto para envío de datos
const long intervaloRutina = 20000;  // Intervalo de 20 segundos para la rutina de relés

int pantallaActual = 0; // Estado de la pantalla para alternar entre datos

// Variables para el control de los relés y el sensor de nivel de agua
const int releNivelBajoPin = 5;
const int releNivelAltoPin = 32;
const int sensorNivelAguaPin = 34;

bool rutinaEjecutando = false;
unsigned long tiempoEsperarNivelBajo = 0;

void setup() {
  Serial.begin(115200);
  Serial.println(F("Iniciando sistema BH1750 y DHT22..."));

  dht.begin();
  Wire.begin(21, 22);
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("Sensor BH1750 iniciado correctamente"));
  } else {
    Serial.println(F("Error al iniciar el sensor BH1750"));
    while (1);
  }

  delay(2000);

  lux = lightMeter.readLightLevel();
  Serial.print("Lectura inicial de luz descartada: ");
  Serial.println(lux);

  lcd.begin(16, 2);
  lcd.backlight();

  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConexión WiFi establecida");
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());

  pinMode(releNivelBajoPin, OUTPUT);
  pinMode(releNivelAltoPin, OUTPUT);
  pinMode(sensorNivelAguaPin, INPUT);

  digitalWrite(releNivelBajoPin, LOW);
  digitalWrite(releNivelAltoPin, LOW);

  // Crear tareas para FreeRTOS
  xTaskCreatePinnedToCore(actualizarPantallaTask, "ActualizarPantalla", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(leerSensoresTask, "LeerSensores", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(envioDatosTask, "EnvioDatos", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(ejecutarRutinaTask, "EjecutarRutina", 2048, NULL, 1, NULL, 1);
}

void loop() {
  // El loop no está haciendo nada, ya que FreeRTOS maneja las tareas
}

void actualizarPantallaTask(void* parameter) {
  while (true) {
    if (pantallaActual == 0) {
      float temp = dht.readTemperature();
      if (!isnan(temp)) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Temp: ");
        lcd.print(temp);
        lcd.print("C");
        Serial.print("Temperatura: ");
        Serial.print(temp);
        Serial.println(" *C");
      }
      pantallaActual = 1;
    } else if (pantallaActual == 1) {
      float humidity = dht.readHumidity();
      if (!isnan(humidity)) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Hum: ");
        lcd.print(humidity);
        lcd.print("%");
        Serial.print("Humedad: ");
        Serial.print(humidity);
        Serial.println(" %");
      }
      pantallaActual = 2;
    } else if (pantallaActual == 2) {
      lux = lightMeter.readLightLevel();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Luz: ");
      lcd.print(lux);
      lcd.print(" lx");
      Serial.print("Luz: ");
      Serial.print(lux);
      Serial.println(" lx");
      pantallaActual = 0;
    }
    delay(2000);  // Intervalo para actualización del LCD
  }
}

void leerSensoresTask(void* parameter) {
  while (true) {
    t = dht.readTemperature();
    h = dht.readHumidity();
    lux = lightMeter.readLightLevel();
    delay(500);  // Leer sensores cada 500ms
  }
}

void envioDatosTask(void* parameter) {
  unsigned long tiempoUltimoEnvio = 0;
  unsigned long intervaloPrimerEnvio = 60000;  // Primer envío después de 1 minuto
  unsigned long intervaloSubsecuente = 900000; // 15 minutos después del primer envío
  
  bool primerEnvio = true;  // Para controlar el primer envío
  
  while (true) {
    unsigned long tiempoActual = millis();
    
    if (primerEnvio) {
      // Primer envío después de 1 minuto
      if (tiempoActual - tiempoUltimoEnvio >= intervaloPrimerEnvio) {
        if (WiFi.status() == WL_CONNECTED) {
          HTTPClient http;

          String datos_a_enviar = "{\"temperatura\":" + String(t) + ",\"humedad\":" + String(h) + ",\"luz\":" + String(lux) + "}";

          Serial.println("Datos a enviar: " + datos_a_enviar);

          http.begin(serverUrl);
          http.addHeader("Content-Type", "application/json");

          int codigo_respuesta = http.POST(datos_a_enviar);

          if (codigo_respuesta > 0) {
            Serial.println("Respuesta del servidor: " + String(codigo_respuesta));
            String respuesta = http.getString();
            Serial.println("Respuesta del servidor: " + respuesta);
          } else {
            Serial.println("Error en la solicitud HTTP: " + String(codigo_respuesta));
          }
          http.end();
        } else {
          Serial.println("Error: No hay conexión WiFi.");
        }
        
        // Después del primer envío, cambiar a intervalo subsecuente
        primerEnvio = false;
        tiempoUltimoEnvio = tiempoActual;  // Actualiza el tiempo del último envío
      }
    } else {
      // Enviar cada 15 minutos después del primer envío
      if (tiempoActual - tiempoUltimoEnvio >= intervaloSubsecuente) {
        if (WiFi.status() == WL_CONNECTED) {
          HTTPClient http;

          String datos_a_enviar = "{\"temperatura\":" + String(t) + ",\"humedad\":" + String(h) + ",\"luz\":" + String(lux) + "}";

          Serial.println("Datos a enviar: " + datos_a_enviar);

          http.begin(serverUrl);
          http.addHeader("Content-Type", "application/json");

          int codigo_respuesta = http.POST(datos_a_enviar);

          if (codigo_respuesta > 0) {
            Serial.println("Respuesta del servidor: " + String(codigo_respuesta));
            String respuesta = http.getString();
            Serial.println("Respuesta del servidor: " + respuesta);
          } else {
            Serial.println("Error en la solicitud HTTP: " + String(codigo_respuesta));
          }
          http.end();
        } else {
          Serial.println("Error: No hay conexión WiFi.");
        }
        tiempoUltimoEnvio = tiempoActual;  // Actualiza el tiempo del último envío
      }
    }
    delay(1000);  // Espera de 1 segundo para el siguiente ciclo de envío
  }
}

void ejecutarRutinaTask(void* parameter) {
  while (true) {
    if (!rutinaEjecutando) {
      rutinaEjecutando = true;
      Serial.println("Iniciando rutina...");

      digitalWrite(releNivelBajoPin, HIGH);
      tiempoEsperarNivelBajo = millis();  // Inicia la espera del sensor de nivel de agua

      // Espera hasta que el sensor de nivel de agua detecte bajo
      while (digitalRead(sensorNivelAguaPin) == LOW) {
        delay(100);
      }
      digitalWrite(releNivelBajoPin, LOW);
      delay(5000);  // Espera de 5 segundos antes de activar el siguiente rele

      digitalWrite(releNivelAltoPin, HIGH);
      delay(5000);  // Espera de 5 segundos

      digitalWrite(releNivelAltoPin, LOW);
      rutinaEjecutando = false;  // Finaliza la rutina
    }
    delay(1000);  // Intervalo pequeño para no bloquear la tarea
  }
}

