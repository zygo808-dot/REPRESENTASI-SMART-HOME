// ===========================================================
//   SMART HOME GABUNGAN — ESP32
//   Proyek yang digabung:
//     1. Deteksi Gas      (MQ6 + Buzzer)
//     2. Lampu Otomatis   (LDR + Relay + LED)
//     3. Monitoring Suhu  (DHT11 + LCD I2C + MQTT)
//     4. Pintu Otomatis   (HC-SR04 + Servo)
//
//   Semua pin & logika dipertahankan sesuai proyek aslinya.
// ===========================================================

// ──────────────────────────────────────────────────────────
//  LIBRARY
// ──────────────────────────────────────────────────────────
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <UniversalTelegramBot.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP32Servo.h>

// ──────────────────────────────────────────────────────────
//  WIFI & TELEGRAM (SHARED)
// ──────────────────────────────────────────────────────────
const char* ssid     = "rdm";
const char* password = "0987654321";

// Token & Chat ID — satu akun bot dipakai bersama semua proyek
const char* botToken = "8551918815:AAHvOJWDkcA83TuO2ajzdZYAaNXhyHB3N7U";       // ganti dengan token bot kamu
const String chatID  = "8521840171";          // ganti dengan chat ID kamu

// Untuk proyek yang pakai HTTPClient (gas & suhu)
String BOT_TOKEN_HTTP = "8551918815:AAHvOJWDkcA83TuO2ajzdZYAaNXhyHB3N7U";      // sama dengan botToken di atas
String CHAT_ID_HTTP   = "8521840171";         // sama dengan chatID di atas

WiFiClientSecure secureClient;
UniversalTelegramBot bot(botToken, secureClient);

// ──────────────────────────────────────────────────────────
//  MQTT  (Monitoring Suhu)
// ──────────────────────────────────────────────────────────
const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_port   = 1883;
const char* mqtt_topic  = "suhu/node";

WiFiClient   mqttEspClient;
PubSubClient mqttClient(mqttEspClient);

// ════════════════════════════════════════════════════════════
//  PROYEK 1 — DETEKSI GAS  (MQ6 + Buzzer)
//  INPUT  : MQ6  → GPIO 32
//  OUTPUT : Buzzer → GPIO 15
// ════════════════════════════════════════════════════════════
#define MQ6_PIN    32
#define BUZZER_PIN 15

unsigned long gas_lastSensorRead  = 0;
unsigned long gas_lastTelegramSent = 0;
const unsigned long gas_telegramDelay = 5000;   // minimal jeda notif gas (ms)

void gas_sendTelegram(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + BOT_TOKEN_HTTP +
                 "/sendMessage?chat_id=" + CHAT_ID_HTTP +
                 "&text=" + message;
    http.begin(url);
    http.GET();
    http.end();
  }
}

void gas_handle() {
  if (millis() - gas_lastSensorRead > 2000) {
    gas_lastSensorRead = millis();

    int gasValue = analogRead(MQ6_PIN);
    Serial.print("[GAS] Nilai MQ6: ");
    Serial.println(gasValue);

    if (gasValue > 2000) {
      digitalWrite(BUZZER_PIN, HIGH);

      if (millis() - gas_lastTelegramSent > gas_telegramDelay) {
        gas_sendTelegram("⚠️ PERINGATAN! Kebocoran GAS terdeteksi. Nilai: " + String(gasValue));
        gas_lastTelegramSent = millis();
      }
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}

// ════════════════════════════════════════════════════════════
//  PROYEK 2 — LAMPU OTOMATIS  (LDR + Relay + LED)
//  INPUT  : LDR   → GPIO 34
//  OUTPUT : Relay → GPIO 26
//           LED   → GPIO 27
// ════════════════════════════════════════════════════════════
#define LDR_PIN   34
#define RELAY_PIN 26
#define LED_PIN   27

// 1 = ACTIVE LOW (kebanyakan modul relay pasar) | 2 = ACTIVE HIGH
#define RELAY_TYPE 1

int  lampu_batasGelap  = 2000;
bool lampu_modeAuto    = true;
bool lampu_lampuState  = false;

unsigned long lampu_lastTelegramCheck = 0;
unsigned long lampu_lastLDRCheck      = 0;
const unsigned long lampu_telegramInterval = 2000;
const unsigned long lampu_ldrInterval     = 500;

void lampu_relayON() {
#if RELAY_TYPE == 1
  digitalWrite(RELAY_PIN, LOW);
#else
  digitalWrite(RELAY_PIN, HIGH);
#endif
  digitalWrite(LED_PIN, HIGH);
  lampu_lampuState = true;
  Serial.println("[LAMPU] >>> Lampu ON");
}

void lampu_relayOFF() {
#if RELAY_TYPE == 1
  digitalWrite(RELAY_PIN, HIGH);
#else
  digitalWrite(RELAY_PIN, LOW);
#endif
  digitalWrite(LED_PIN, LOW);
  lampu_lampuState = false;
  Serial.println("[LAMPU] >>> Lampu OFF");
}

void lampu_handleTelegramMessage(String text, String from) {
  if (from == chatID) {
    text.toLowerCase();
    text.trim();

    if (text == "lampu on") {
      lampu_modeAuto = false;
      lampu_relayON();
      bot.sendMessage(chatID, "💡 Lampu ON (manual)", "");
    }
    else if (text == "lampu off") {
      lampu_modeAuto = false;
      lampu_relayOFF();
      bot.sendMessage(chatID, "🌑 Lampu OFF (manual)", "");
    }
    else if (text == "lampu auto") {
      lampu_modeAuto = true;
      int ldrNow = analogRead(LDR_PIN);
      if (ldrNow > lampu_batasGelap) lampu_relayON();
      else                            lampu_relayOFF();
      bot.sendMessage(chatID, "⚙️ Mode AUTO aktif\nRelay disesuaikan dengan kondisi cahaya.", "");
    }
    else if (text == "lampu status") {
      String ldrVal    = String(analogRead(LDR_PIN));
      String statusMsg = "📊 *Status Lampu*\n";
      statusMsg += "Mode  : " + String(lampu_modeAuto ? "AUTO" : "MANUAL") + "\n";
      statusMsg += "Lampu : " + String(lampu_lampuState ? "ON 💡" : "OFF 🌑") + "\n";
      statusMsg += "LDR   : " + ldrVal + "\n";
      statusMsg += "Batas : " + String(lampu_batasGelap);
      bot.sendMessage(chatID, statusMsg, "Markdown");
    }
  }
}

void lampu_handleLDR() {
  if (lampu_modeAuto && (millis() - lampu_lastLDRCheck >= lampu_ldrInterval)) {
    lampu_lastLDRCheck = millis();
    int ldrValue = analogRead(LDR_PIN);
    Serial.print("[LAMPU] LDR: ");
    Serial.println(ldrValue);

    if (ldrValue > lampu_batasGelap && !lampu_lampuState) lampu_relayON();
    else if (ldrValue <= lampu_batasGelap && lampu_lampuState) lampu_relayOFF();
  }
}

// ════════════════════════════════════════════════════════════
//  PROYEK 3 — MONITORING SUHU  (DHT11 + LCD I2C + MQTT)
//  INPUT  : DHT11 → GPIO 4
//  OUTPUT : LCD I2C (0x27, 16x2) — SDA/SCL default ESP32
//           MQTT publish ke "suhu/node"
//           Telegram notif jika suhu > 32°C
// ════════════════════════════════════════════════════════════
#define DHT_PIN  4
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long suhu_lastSensorRead  = 0;
unsigned long suhu_lastMQTTAttempt = 0;
unsigned long suhu_lastTelegramSent = 0;
const unsigned long suhu_telegramDelay = 30000;  // jeda notif suhu 30 detik

void suhu_sendTelegram(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + BOT_TOKEN_HTTP +
                 "/sendMessage?chat_id=" + CHAT_ID_HTTP +
                 "&text=" + message;
    http.begin(url);
    http.GET();
    http.end();
  }
}

void suhu_reconnectMQTT() {
  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    if (millis() - suhu_lastMQTTAttempt > 5000) {
      suhu_lastMQTTAttempt = millis();
      mqttClient.connect("ESP32_DHT_Client");
    }
  }
}

void suhu_handle() {
  suhu_reconnectMQTT();
  mqttClient.loop();

  if (millis() - suhu_lastSensorRead > 3000) {
    suhu_lastSensorRead = millis();

    float temperature = dht.readTemperature();
    float humidity    = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("[SUHU] Gagal baca DHT");
      return;
    }

    Serial.print("[SUHU] Temp: "); Serial.print(temperature);
    Serial.print("°C  | Humi: "); Serial.print(humidity); Serial.println("%");

    // LCD
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperature);
    lcd.print("C   ");

    lcd.setCursor(0, 1);
    lcd.print("H:");
    lcd.print(humidity);
    lcd.print("%   ");

    // MQTT
    if (mqttClient.connected()) {
      String payload = String(temperature) + "," + String(humidity);
      mqttClient.publish(mqtt_topic, payload.c_str());
    }

    // Telegram notif suhu ekstrim
    if (temperature > 32) {
      if (millis() - suhu_lastTelegramSent > suhu_telegramDelay) {
        suhu_sendTelegram("⚠️ Suhu Ekstrim: " + String(temperature) + "°C | Kelembaban: " + String(humidity) + "%");
        suhu_lastTelegramSent = millis();
      }
    }
  }
}

// ════════════════════════════════════════════════════════════
//  PROYEK 4 — PINTU OTOMATIS  (HC-SR04 + Servo)
//  INPUT  : HC-SR04  Trig → GPIO 5 | Echo → GPIO 18
//  OUTPUT : Servo    → GPIO 13
// ════════════════════════════════════════════════════════════
Servo pintuServo;
const int servoPin = 13;
const int trigPin  = 5;
const int echoPin  = 18;

bool pintu_isLocked    = false;
bool pintu_terbuka     = false;

const int jarakBuka  = 15;   // cm — pintu buka jika jarak < ini
const int jarakTutup = 20;   // cm — pintu tutup jika jarak > ini

void bukaPintu() {
  pintuServo.write(90);
  Serial.println("[PINTU] Pintu Terbuka");
}

void tutupPintu() {
  pintuServo.write(0);
  Serial.println("[PINTU] Pintu Tertutup");
}

long bacaJarak() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long durasi = pulseIn(echoPin, HIGH, 30000);
  if (durasi == 0) return -1;
  return durasi * 0.034 / 2;
}

void pintu_handleTelegramMessage(String text, String from) {
  if (from == chatID) {
    text.toLowerCase();

    if (text == "kunci") {
      pintu_isLocked = true;
      tutupPintu();
      pintu_terbuka = false;
      bot.sendMessage(chatID, "🔒 Pintu dikunci.", "");
    }
    else if (text == "buka") {
      pintu_isLocked = false;
      bot.sendMessage(chatID, "🔓 Mode otomatis pintu aktif.", "");
    }
  }
}

void pintu_handle() {
  if (!pintu_isLocked) {
    long jarak = bacaJarak();

    if (jarak != -1) {
      Serial.print("[PINTU] Jarak: ");
      Serial.println(jarak);

      if (jarak < jarakBuka && !pintu_terbuka) {
        bukaPintu();
        pintu_terbuka = true;
      }
      else if (jarak > jarakTutup && pintu_terbuka) {
        tutupPintu();
        pintu_terbuka = false;
      }
    }
  }
}

// ──────────────────────────────────────────────────────────
//  TELEGRAM — POLLING TERPUSAT
//  Semua perintah bot diproses di sini lalu diteruskan
//  ke handler masing-masing proyek.
//
//  Daftar perintah:
//    Lampu  : lampu on | lampu off | lampu auto | lampu status
//    Pintu  : kunci | buka
// ──────────────────────────────────────────────────────────
unsigned long telegram_lastCheck = 0;
const unsigned long telegramCheckInterval = 2000;

void telegram_pollMessages() {
  if (millis() - telegram_lastCheck >= telegramCheckInterval) {
    telegram_lastCheck = millis();

    int numNew = bot.getUpdates(bot.last_message_received + 1);
    while (numNew) {
      for (int i = 0; i < numNew; i++) {
        String text = bot.messages[i].text;
        String from = bot.messages[i].from_id;

        // Teruskan ke proyek yang relevan
        lampu_handleTelegramMessage(text, from);
        pintu_handleTelegramMessage(text, from);
      }
      numNew = bot.getUpdates(bot.last_message_received + 1);
    }
  }
}

// ──────────────────────────────────────────────────────────
//  WIFI RECONNECT (SHARED)
// ──────────────────────────────────────────────────────────
unsigned long wifi_lastAttempt = 0;

void wifi_reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifi_lastAttempt > 5000) {
      wifi_lastAttempt = millis();
      WiFi.begin(ssid, password);
      Serial.println("[WiFi] Mencoba reconnect...");
    }
  }
}

// ══════════════════════════════════════════════════════════
//  SETUP
// ══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  // ── WiFi ──
  WiFi.begin(ssid, password);
  Serial.print("[WiFi] Menghubungkan");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Terhubung! IP: " + WiFi.localIP().toString());

  // ── Telegram (UniversalTelegramBot) ──
  secureClient.setInsecure();

  // ── MQTT ──
  mqttClient.setServer(mqtt_server, mqtt_port);

  // ── Proyek 1: Deteksi Gas ──
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // ── Proyek 2: Lampu Otomatis ──
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  lampu_relayOFF();   // relay mati saat boot

  // ── Proyek 3: Monitoring Suhu ──
  lcd.init();
  lcd.backlight();
  dht.begin();

  // ── Proyek 4: Pintu Otomatis ──
  pintuServo.attach(servoPin);
  tutupPintu();
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  Serial.println("=== Semua sistem siap ===");
}

// ══════════════════════════════════════════════════════════
//  LOOP
// ══════════════════════════════════════════════════════════
void loop() {

  wifi_reconnect();           // jaga koneksi WiFi

  gas_handle();               // Proyek 1: baca MQ6, kontrol buzzer
  lampu_handleLDR();          // Proyek 2: baca LDR, kontrol relay/LED
  suhu_handle();              // Proyek 3: baca DHT, tampil LCD, kirim MQTT
  pintu_handle();             // Proyek 4: baca ultrasonik, kontrol servo

  telegram_pollMessages();    // Polling Telegram terpusat (semua proyek)
}
