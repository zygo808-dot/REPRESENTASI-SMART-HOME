#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <HTTPClient.h>

// ================= WIFI =================
const char* ssid = "rdm";
const char* password = "0987654321";

// ================= MQTT =================
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* topic = "suhu/node";

// ================= TELEGRAM =================
String BOT_TOKEN = "toket";
String CHAT_ID  = "id";

// ================= DHT =================
#define DHT_PIN 4
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ================= LCD =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= MQTT CLIENT =================
WiFiClient espClient;
PubSubClient client(espClient);

// ================= TIMER =================
unsigned long lastSensorRead = 0;
unsigned long lastMQTTAttempt = 0;
unsigned long lastWiFiAttempt = 0;

// ================= DELAY TELEGRAM =================
unsigned long lastTelegramSent = 0;
const unsigned long telegramDelay = 30000; //  detik

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();

  dht.begin();

  WiFi.begin(ssid, password);
  client.setServer(mqtt_server, mqtt_port);
}

// ================= TELEGRAM FUNCTION =================
void sendTelegram(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage?chat_id=" + CHAT_ID + "&text=" + message;

    http.begin(url);
    http.GET();
    http.end();
  }
}

// ================= LOOP =================
void loop() {

  // ===== RECONNECT WIFI =====
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWiFiAttempt > 5000) {
      lastWiFiAttempt = millis();
      WiFi.begin(ssid, password);
    }
  }

  // ===== RECONNECT MQTT =====
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    if (millis() - lastMQTTAttempt > 5000) {
      lastMQTTAttempt = millis();
      client.connect("ESP32_DHT_Client");
    }
  }

  client.loop();

  // ===== BACA SENSOR =====
  if (millis() - lastSensorRead > 3000) {
    lastSensorRead = millis();

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Gagal baca DHT");
      return;
    }

    // ===== LCD =====
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperature);
    lcd.print("C   ");

    lcd.setCursor(0, 1);
    lcd.print("H:");
    lcd.print(humidity);
    lcd.print("%   ");

    // ===== MQTT =====
    if (client.connected()) {
      String payload = String(temperature) + "," + String(humidity);
      client.publish(topic, payload.c_str());
    }

    // ===== TELEGRAM NOTIF (PAKAI DELAY) =====
    if (temperature > 32) {
      if (millis() - lastTelegramSent > telegramDelay) {
        sendTelegram("⚠️ Suhu Ekstrim: " + String(temperature) + " C");
        lastTelegramSent = millis();
      }
    }
  }
}
