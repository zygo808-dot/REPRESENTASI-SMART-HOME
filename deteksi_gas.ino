#include <WiFi.h>
#include <HTTPClient.h>

// ================= WIFI =================
const char* ssid = "rdm";
const char* password = "0987654321";

// ================= TELEGRAM =================
String BOT_TOKEN = "8551918815:AAHvOJWDkcA83TuO2ajzdZYAaNXhyHB3N7U";
String CHAT_ID  = "8521840171";

// ================= MQ6 =================
#define MQ6_PIN 34

// ================= BUZZER =================
#define BUZZER_PIN 15

// ================= TIMER =================
unsigned long lastSensorRead = 0;
unsigned long lastTelegramSent = 0;

const unsigned long telegramDelay = 5000;

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.begin(ssid, password);
}

// ================= TELEGRAM FUNCTION =================
void sendTelegram(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = "https://api.telegram.org/bot" + BOT_TOKEN +
                 "/sendMessage?chat_id=" + CHAT_ID +
                 "&text=" + message;

    http.begin(url);
    http.GET();
    http.end();
  }
}

// ================= LOOP =================
void loop() {

  // ===== WIFI RECONNECT =====
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    delay(2000);
  }

  // ===== BACA SENSOR =====
  if (millis() - lastSensorRead > 2000) {
    lastSensorRead = millis();

    int gasValue = analogRead(MQ6_PIN);
    Serial.println(gasValue);

    // ===== KONDISI BAHAYA =====
    if (gasValue > 2000) {
      digitalWrite(BUZZER_PIN, HIGH);

      if (millis() - lastTelegramSent > telegramDelay) {
        sendTelegram("⚠️ PERINGATAN! Kebocoran GAS terdeteksi. Nilai: " + String(gasValue));
        lastTelegramSent = millis();
      }
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}