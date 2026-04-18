#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESP32Servo.h>

// ====== WiFi & Telegram ======
const char* ssid = "rdm";
const char* password = "0987654321";
const char* botToken = "token"; // ganti token
const String chatID = "id";

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

// ====== Komponen ======
Servo pintuServo;
const int servoPin = 13;
const int trigPin = 5;
const int echoPin = 18;

// ====== Parameter ======
bool isLocked = false;
bool pintuTerbuka = false;

unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 2000;

// Hysteresis
const int jarakBuka = 15;
const int jarakTutup = 20;

// ====== Servo ======
void bukaPintu() {
  pintuServo.write(90);
  Serial.println("Pintu Terbuka");
}

void tutupPintu() {
  pintuServo.write(0);
  Serial.println("Pintu Tertutup");
}

// ====== Ultrasonik ======
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

// ====== Telegram ======
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    String from = bot.messages[i].from_id;

    if (from == chatID) {
      text.toLowerCase();

      if (text == "kunci") {
        isLocked = true;
        tutupPintu();
        pintuTerbuka = false;
        bot.sendMessage(chatID, "🔒 Pintu dikunci.", "");
      }

      else if (text == "buka") {
        isLocked = false;
        bot.sendMessage(chatID, "🔓 Mode otomatis aktif.", "");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);

  pintuServo.attach(servoPin);
  tutupPintu();

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  client.setInsecure();
}

void loop() {
  // ====== Mode Otomatis ======
  if (!isLocked) {
    long jarak = bacaJarak();

    if (jarak != -1) {
      Serial.print("Jarak: ");
      Serial.println(jarak);

      if (jarak < jarakBuka && !pintuTerbuka) {
        bukaPintu();
        pintuTerbuka = true;
      }

      else if (jarak > jarakTutup && pintuTerbuka) {
        tutupPintu();
        pintuTerbuka = false;
      }
    }
  }

  // ====== Telegram ======
  if (millis() - lastCheckTime > checkInterval) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    lastCheckTime = millis();
  }

  delay(200);
}
