#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <DHT.h>
#include <WiFiClientSecure.h>

// --- REDES WIFI ---
const char* ssid1 = "Stefany";
const char* password1 = "estefani123";

const char* ssid2 = "WIFI-ITM"; // Red abierta
const char* password2 = "";

// --- TELEGRAM ---
const char* telegramToken = "7887445109:AAHKvCpGMvUOVJKs9UTQ24OpI0eb3rvM2Yw"; 

// IDs de chat para enviar mensajes
const char* chatIDs[] = {"7044113532", "1327947146"};
const int numChats = 2;

// --- SENSOR DHT11 ---
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- LCD ---
LiquidCrystal_PCF8574 lcd(0x27);
#define LCD_COLUMNS 16
#define LCD_ROWS 2

// --- UMBRALES ---
const float TEMP_MIN = 15.0;
const float TEMP_MAX = 30.0;

// --- VARIABLES GLOBALES ---
float lastTemp = NAN;
float lastHum = NAN;
unsigned long lastTelegram = 0;
const unsigned long telegramInterval = 60000; // 1 minuto

WebServer server(80);

// --- FUNCIÓN PARA CODIFICAR URL ---
String urlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += "+";
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

// --- CONECTAR WIFI ---
void conectarWiFi() {
  Serial.println("🔌 Intentando conectar a Stefany...");
  WiFi.begin(ssid1, password1);
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 10) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n⚠️ No se pudo conectar a Stefany. Probando WIFI-ITM...");
    WiFi.begin(ssid2, password2);
    intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 10) {
      delay(500);
      Serial.print(".");
      intentos++;
    }
  }

  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Conectado a WiFi");
    Serial.print("🟢 IP: ");
    Serial.println(WiFi.localIP());

    lcd.setCursor(0, 0);
    lcd.print("WiFi Conectado!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());
  } else {
    Serial.println("\n❌ Falló conexión WiFi");
    lcd.print("❌ Error WiFi");
  }

  delay(2000);
  lcd.clear();
}

// --- ENVIAR MENSAJE TELEGRAM ---
void enviarMensajeTelegram(String mensaje) {
  String mensajeCodificado = urlEncode(mensaje);

  for (int i = 0; i < numChats; i++) {
    WiFiClientSecure client;
    client.setInsecure();

    Serial.println("Conectando a Telegram...");
    if (!client.connect("api.telegram.org", 443)) {
      Serial.println("❌ Error: No se pudo conectar con Telegram");
      continue;
    }

    String url = "/bot" + String(telegramToken) + "/sendMessage?chat_id=" + String(chatIDs[i]) + "&text=" + mensajeCodificado;
    Serial.println("Enviando petición: " + url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: api.telegram.org\r\n" +
                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    bool responseReceived = false;
    while (client.connected() && (millis() - timeout < 5000)) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        Serial.println(line);
        if (line.indexOf("\"ok\":true") != -1) {
          Serial.println("✅ Mensaje enviado correctamente para chat ID: " + String(chatIDs[i]));
          responseReceived = true;
          break;
        }
        if (line.startsWith("{\"ok\":false")) {
          Serial.println("⚠️ Error al enviar mensaje para chat ID: " + String(chatIDs[i]));
          responseReceived = true;
          break;
        }
      }
    }
    if (!responseReceived) {
      Serial.println("⚠️ Sin respuesta o error desconocido al enviar mensaje para chat ID: " + String(chatIDs[i]));
    }
    
    client.stop();
    delay(1000);
  }
}

// --- MOSTRAR EN LCD ---
void mostrarEnLCD(float temp, float hum) {
  lcd.clear();
  if (isnan(temp) || isnan(hum)) {
    lcd.setCursor(0, 0);
    lcd.print("Error sensor DHT");
    return;
  }

  lcd.setCursor(0, 0);
  lcd.printf("Temp: %.1f%cC", temp, (char)223);
  lcd.setCursor(0, 1);

  if (temp < TEMP_MIN) {
    lcd.print("Temp. MUY BAJA!");
  } else if (temp > TEMP_MAX) {
    lcd.print("Temp. MUY ALTA!");
  } else {
    lcd.printf("Hum: %.0f%%", hum);
  }
}

// --- SERVIDOR HTTP ---
void handleRoot() {
  StaticJsonDocument<200> doc;
  server.sendHeader("Access-Control-Allow-Origin", "*");

  if (isnan(lastTemp) || isnan(lastHum)) {
    doc["error"] = "Sensor no disponible";
  } else {
    doc["temperature"] = lastTemp;
    doc["humidity"] = lastHum;
  }

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  Wire.begin(21,22);
  lcd.begin(LCD_COLUMNS, LCD_ROWS);
  lcd.setBacklight(true);

  lcd.setCursor(0, 0);
  lcd.print("Estefania \xF0\x9F\x92\x9C"); // Corazón emoji UTF-8
  delay(2000);
  lcd.clear();

  dht.begin();
  conectarWiFi();

  server.on("/", handleRoot);
  server.begin();

  Serial.println("🌐 Servidor HTTP listo en puerto 80");
}

// --- LOOP PRINCIPAL ---
void loop() {
  server.handleClient();

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  mostrarEnLCD(temp, hum);

  if (!isnan(temp) && !isnan(hum)) {
    lastTemp = temp;
    lastHum = hum;

    Serial.printf("🌡 Temp: %.1f°C | 💧 Hum: %.0f%%\n", temp, hum);

    unsigned long now = millis();
    if ((temp < TEMP_MIN || temp > TEMP_MAX) && (now - lastTelegram > telegramInterval)) {
      String mensaje = "🚨 Alerta de Temperatura 🚨\n\nTemperatura: " + String(temp, 1) + " C\nHumedad: " + String(hum, 0) + " %";
      enviarMensajeTelegram(mensaje);
      lastTelegram = now;
    }
  } else {
    Serial.println("⚠️ Error leyendo el sensor DHT11");
  }

  delay(3000);
}