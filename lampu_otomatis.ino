#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// ====== WiFi & Telegram ======
const char* ssid = "rdm";
const char* password = "0987654321";
const char* botToken = "8551918815:AAHvOJWDkcA83TuO2ajzdZYAaNXhyHB3N7U"; // ganti token
const String chatID = "8521840171";

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

// ====== PIN ======
#define LDR_PIN   34
#define RELAY_PIN 26
#define LED_PIN   27

// ====== SETTING ======
int batasGelap = 2000;
bool modeAuto = true;
bool lampuState = false;

// ====== TIMING ======
unsigned long lastTelegramCheck = 0;
unsigned long lastLDRCheck = 0;
const unsigned long telegramInterval = 2000;  // cek Telegram tiap 2 detik
const unsigned long ldrInterval = 500;        // cek LDR tiap 500ms

// =================================================
// 🔥 PILIH TIPE RELAY DI SINI
// 1 = ACTIVE LOW  (kebanyakan modul relay pasar)
// 2 = ACTIVE HIGH
// =================================================
#define RELAY_TYPE 1

// ====== CONTROL RELAY + LED ======
void relayON() {
#if RELAY_TYPE == 1
  digitalWrite(RELAY_PIN, LOW);   // Active LOW → nyalakan relay
#else
  digitalWrite(RELAY_PIN, HIGH);
#endif
  digitalWrite(LED_PIN, HIGH);    // LED indikator ikut nyala
  lampuState = true;
  Serial.println(">>> Lampu ON");
}

void relayOFF() {
#if RELAY_TYPE == 1
  digitalWrite(RELAY_PIN, HIGH);  // Active LOW → matikan relay
#else
  digitalWrite(RELAY_PIN, LOW);
#endif
  digitalWrite(LED_PIN, LOW);     // LED indikator ikut mati
  lampuState = false;
  Serial.println(">>> Lampu OFF");
}

// ====== HANDLER TELEGRAM ======
void handleMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    String from = bot.messages[i].from_id;

    Serial.print("Pesan dari: ");
    Serial.print(from);
    Serial.print(" | Isi: ");
    Serial.println(text);

    if (from == chatID) {
      text.toLowerCase();
      text.trim();  // ← FIX: hapus spasi/newline yang bikin perintah tidak terbaca

      if (text == "on") {
        modeAuto = false;
        relayON();
        bot.sendMessage(chatID, "💡 Lampu ON (manual)", "");
      }
      else if (text == "off") {
        modeAuto = false;
        relayOFF();
        bot.sendMessage(chatID, "🌑 Lampu OFF (manual)", "");
      }
      else if (text == "auto") {
        modeAuto = true;
        // ← FIX: langsung baca LDR dan sesuaikan kondisi relay saat masuk auto
        int ldrNow = analogRead(LDR_PIN);
        if (ldrNow > batasGelap) {
          relayON();
        } else {
          relayOFF();
        }
        bot.sendMessage(chatID, "⚙️ Mode AUTO aktif\nRelay disesuaikan dengan kondisi cahaya.", "");
      }
      else if (text == "status") {
        String ldrVal = String(analogRead(LDR_PIN));
        String statusMsg = "📊 *Status Sistem*\n";
        statusMsg += "Mode   : " + String(modeAuto ? "AUTO" : "MANUAL") + "\n";
        statusMsg += "Lampu  : " + String(lampuState ? "ON 💡" : "OFF 🌑") + "\n";
        statusMsg += "LDR    : " + ldrVal + "\n";
        statusMsg += "Batas  : " + String(batasGelap);
        bot.sendMessage(chatID, statusMsg, "Markdown");
      }
      else {
        bot.sendMessage(chatID,
          "❓ Perintah tidak dikenal.\nGunakan:\n• *on* → nyalakan lampu\n• *off* → matikan lampu\n• *auto* → mode otomatis\n• *status* → cek kondisi",
          "Markdown");
      }
    }
  }
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  relayOFF();  // pastikan relay mati saat boot

  // Koneksi WiFi
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung! IP: " + WiFi.localIP().toString());

  client.setInsecure();
  Serial.println("Bot Telegram siap.");
  Serial.println("Mode default: AUTO");
}

// ====== LOOP ======
void loop() {

  // ====== CEK LDR (mode auto, interval 500ms) ======
  if (modeAuto && (millis() - lastLDRCheck >= ldrInterval)) {
    lastLDRCheck = millis();

    int ldrValue = analogRead(LDR_PIN);
    Serial.print("LDR: ");
    Serial.println(ldrValue);

    if (ldrValue > batasGelap && !lampuState) {
      relayON();   // Gelap → nyalakan relay (lampu AC)
    }
    else if (ldrValue <= batasGelap && lampuState) {
      relayOFF();  // Terang → matikan relay (lampu AC)
    }
  }

  // ====== CEK TELEGRAM (interval 2 detik, tanpa blocking delay) ======
  if (millis() - lastTelegramCheck >= telegramInterval) {
    lastTelegramCheck = millis();

    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
  }

  // ← TIDAK ADA delay() di sini agar loop responsif
}