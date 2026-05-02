#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ==================== CONFIGURATION ====================
static const char *WIFI_SSID = "{[Enter your Wifi Name]}";
static const char *WIFI_PASSWORD = "[Enter your Wifi password]";
static const char *BOT_TOKEN = "[enter the token given by the botFather(telegram)]";
static const String SECRET_PASSPHRASE = "[choose a pass to enter the system using telegramBot]"; 

String storedChatId = ""; 
bool isAuthorized = false;

// Hardware Pins (ESP32)
static const int STM32_RX_PIN = 16; 
static const int STM32_TX_PIN = 17; 

// Global Sensor Variables
float latestTemp = 0.0, latestHum = 0.0;
bool latestMotion = false;
bool hasData = false;
String rxLine = "";
String latestDoorState = "UNKNOWN";

WiFiClientSecure secureClient;
UniversalTelegramBot bot(BOT_TOKEN, secureClient);

unsigned long lastBotPoll = 0;
const unsigned long BOT_POLL_INTERVAL = 1000;
int lastUpdateId = 0;

static String normalizeCommand(String text) {
  text.trim();
  text.toLowerCase();

  int atPos = text.indexOf('@');
  if (atPos > 0) {
    text = text.substring(0, atPos);
  }
  return text;
}

// ==================== STM32 DATA PARSING ====================
void handleUartLine(String line) {
  line.trim();

  if (line.startsWith("T=")) {
    int m;
    if (sscanf(line.c_str(), "T=%f,H=%f,M=%d", &latestTemp, &latestHum, &m) == 3) {
      latestMotion = (m != 0);
      hasData = true;
    }
  } else if (line == "LOCKED") {
    latestDoorState = "LOCKED";
    Serial.println("STM32 ACK: LOCKED");
  } else if (line == "UNLOCKED") {
    latestDoorState = "UNLOCKED";
    Serial.println("STM32 ACK: UNLOCKED");
  } else if (line == "PIR=MOTION") {
    if (isAuthorized && storedChatId != "") {
      bot.sendMessage(storedChatId, "🚨 ALERT: Motion detected!", "");
    }
  }
}

// ==================== TELEGRAM HANDLER ====================
void handleTelegramMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String message_id = String(bot.messages[i].message_id);
    
    Serial.println("Msg from [" + chat_id + "]: " + text);

    if (!isAuthorized) {
      if (text == SECRET_PASSPHRASE) {
        // --- UPDATED DELETION LOGIC ---
        // Some versions of the library need the full path or better formatting
        String response = bot.sendGetToTelegram("deleteMessage?chat_id=" + chat_id + "&message_id=" + message_id);
        Serial.println("Delete Response: " + response); // Check Serial Monitor to see why it fails
        
        storedChatId = chat_id;
        isAuthorized = true;
        bot.sendMessage(chat_id, "✅ Identity Verified. Password deleted (hopefully).\n\n/lock, /unlock, /data, /logout", "");
      } else {
        bot.sendMessage(chat_id, "🔒 System Locked. Send Passphrase.", "");
      }
      continue;
    }

    if (chat_id != storedChatId) {
      bot.sendMessage(chat_id, "⛔ Access Denied.", "");
      continue;
    }

    text = normalizeCommand(text);
    if (text == "/data" || text == "/status") {
      if (hasData) {
        bot.sendMessage(chat_id, "📊 Status:\nTemp: " + String(latestTemp, 1) + "°C\nHum: " + String(latestHum, 1) + "%\nMotion: " + String(latestMotion ? "YES" : "NO") + "\nDoor: " + latestDoorState, "");
      } else {
        bot.sendMessage(chat_id, "Waiting for data...", "");
      }
    } 
    else if (text == "/lock") {
      Serial2.print("LOCK\r\n");
      Serial.println("TX->STM32: LOCK");
      bot.sendMessage(chat_id, "🔒 Lock sent.", "");
    } 
    else if (text == "/unlock") {
      Serial2.print("UNLOCK\r\n");
      Serial.println("TX->STM32: UNLOCK");
      bot.sendMessage(chat_id, "🔓 Unlock sent.", "");
    } 
    else if (text == "/logout") {
      isAuthorized = false;
      storedChatId = "";
      bot.sendMessage(chat_id, "🚪 Logged out.", "");
    } else {
      bot.sendMessage(chat_id, "Unknown command. Use /data /lock /unlock /logout", "");
    }
  }
}

// ==================== SETUP & LOOP ====================
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Connected");

  secureClient.setInsecure();
}

void loop() {
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n') {
      if (rxLine.length() > 0) { handleUartLine(rxLine); rxLine = ""; }
    } else if (c != '\r') { rxLine += c; }
  }

  if (millis() - lastBotPoll > BOT_POLL_INTERVAL) {
    int numNewMessages = bot.getUpdates(lastUpdateId + 1);
    if (numNewMessages > 0) {
      handleTelegramMessages(numNewMessages);
      lastUpdateId = bot.last_message_received; 
    }
    lastBotPoll = millis();
  }
}