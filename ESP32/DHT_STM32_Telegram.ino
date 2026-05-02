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
z
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
}#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <WebServer.h> 

// ==================== CONFIGURATION ====================
static const char *WIFI_SSID = "testnet2";
static const char *WIFI_PASSWORD = "123456798";
static const char *BOT_TOKEN = "8644046923:AAG1BCx29vcPW5bHi03OLxpGET9PNn42lXk";
static const String SECRET_PASSPHRASE = "Sowad123"; 

String storedChatId = ""; 
bool isAuthorized = false;

// Hardware Pins (ESP32)
static const int STM32_RX_PIN = 16; 
static const int STM32_TX_PIN = 17; 

// Global Sensor Variables
float latestTemp = 0.0f, latestHum = 0.0f;
float latestGas = 0.0f;
bool latestMotion = false;
bool latestEmergency = false;
bool latestLockdown = false;
bool latestOccupancy = true;
bool latestClimate = true;
bool hasData = false;
String rxLine = "";
String latestDoorState = "UNKNOWN";
String latestAlarm = "NORMAL";
String latestMode = "AUTO";

WiFiClientSecure secureClient;
UniversalTelegramBot bot(BOT_TOKEN, secureClient);
WebServer server(80); 

unsigned long lastBotPoll = 0;
const unsigned long BOT_POLL_INTERVAL = 1000;
int lastUpdateId = 0;

// ==================== WEB SERVER UI & HANDLERS ====================

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>@import url('https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@400;600;700&display=swap');";
  html += ":root{--bg1:#0b1117;--bg2:#15202c;--card:#10161f;--text:#e6edf3;--muted:#9fb2c3;--accent:#00b3a4;--accent2:#ffb347;--danger:#ff5d5d;}";
  html += "body{margin:0;font-family:'Space Grotesk',sans-serif;background:radial-gradient(circle at top,var(--bg2),var(--bg1));color:var(--text);text-align:center;padding:20px;}";
  html += "h1{margin:6px 0 18px;font-size:26px;letter-spacing:4px;text-transform:uppercase;}";
  html += ".grid{display:flex;flex-wrap:wrap;gap:12px;justify-content:center;}";
  html += ".card{background:var(--card);padding:16px 18px;border-radius:14px;box-shadow:0 10px 24px rgba(0,0,0,0.4);min-width:170px;border:1px solid rgba(255,255,255,0.06);}";
  html += ".label{letter-spacing:2px;font-size:11px;color:var(--muted);}";
  html += ".value{font-size:26px;font-weight:600;margin-top:6px;}";
  html += ".controls{margin-top:22px;display:flex;flex-wrap:wrap;gap:10px;justify-content:center;}";
  html += "button{padding:12px 18px;font-size:12px;font-weight:700;letter-spacing:1px;border:none;border-radius:8px;cursor:pointer;color:#0b1117;text-transform:uppercase;transition:transform .15s ease,opacity .2s;}";
  html += "button:active{transform:scale(0.98);}button:hover{opacity:0.9;}";
  html += ".btn-primary{background:var(--accent);} .btn-warn{background:var(--accent2);} .btn-danger{background:var(--danger);color:#fff;} .btn-ghost{background:#2a3340;color:#c9d4dd;}";
  html += "</style></head><body>";

  html += "<h1>HOME CORE</h1>";
  html += "<div class='grid'>";
  html += "<div class='card'><div class='label'>TEMP</div><div class='value'>" + String(latestTemp, 1) + "°C</div></div>";
  html += "<div class='card'><div class='label'>HUM</div><div class='value'>" + String(latestHum, 1) + "%</div></div>";
  html += "<div class='card'><div class='label'>MOTION</div><div class='value'>" + String(latestMotion ? "YES" : "NO") + "</div></div>";
  html += "<div class='card'><div class='label'>GAS</div><div class='value'>" + String(latestGas, 0) + "</div></div>";
  html += "<div class='card'><div class='label'>MODE</div><div class='value'>" + latestMode + "</div></div>";
  html += "<div class='card'><div class='label'>ALARM</div><div class='value'>" + latestAlarm + "</div></div>";
  html += "<div class='card'><div class='label'>DOOR</div><div class='value'>" + latestDoorState + "</div></div>";
  html += "</div>";

  html += "<div class='controls'>";
  html += "<button class='btn-primary' onclick=\"location.href='/lock'\">LOCK</button>";
  html += "<button class='btn-primary' onclick=\"location.href='/unlock'\">UNLOCK</button>";
  html += "<button class='btn-ghost' onclick=\"location.href='/doorunlock'\">PULSE UNLOCK</button>";
  html += "<button class='btn-ghost' onclick=\"location.href='/lighton'\">LIGHT ON</button>";
  html += "<button class='btn-ghost' onclick=\"location.href='/lightoff'\">LIGHT OFF</button>";
  html += "<button class='btn-ghost' onclick=\"location.href='/fanon'\">FAN ON</button>";
  html += "<button class='btn-ghost' onclick=\"location.href='/fanoff'\">FAN OFF</button>";
  html += "<button class='btn-warn' onclick=\"location.href='/lockdownon'\">LOCKDOWN ON</button>";
  html += "<button class='btn-warn' onclick=\"location.href='/lockdownoff'\">LOCKDOWN OFF</button>";
  html += "<button class='btn-primary' onclick=\"location.href='/occupancyon'\">OCCUPANCY ON</button>";
  html += "<button class='btn-primary' onclick=\"location.href='/occupancyoff'\">OCCUPANCY OFF</button>";
  html += "<button class='btn-primary' onclick=\"location.href='/climateon'\">CLIMATE ON</button>";
  html += "<button class='btn-primary' onclick=\"location.href='/climateoff'\">CLIMATE OFF</button>";
  html += "<button class='btn-danger' onclick=\"location.href='/clearalarm'\">CLEAR ALARM</button>";
  html += "</div>";

  html += "<script>setTimeout(function(){ location.reload(); }, 4000);</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Web Handlers with Console Logging
void handleLock() { 
  Serial.println("WEB_REQ -> TX: LOCK"); 
  Serial2.print("LOCK\r\n"); 
  server.sendHeader("Location", "/"); 
  server.send(303); 
}

void handleUnlock() { 
  Serial.println("WEB_REQ -> TX: UNLOCK"); 
  Serial2.print("UNLOCK\r\n"); 
  server.sendHeader("Location", "/"); 
  server.send(303); 
}

void handleFanOn() { 
  Serial.println("WEB_REQ -> TX: FANON"); 
  Serial2.print("FANON\r\n"); 
  server.sendHeader("Location", "/"); 
  server.send(303); 
}

void handleFanOff() { 
  Serial.println("WEB_REQ -> TX: FANOFF"); 
  Serial2.print("FANOFF\r\n"); 
  server.sendHeader("Location", "/"); 
  server.send(303); 
}

void handleLightOn() {
  Serial.println("WEB_REQ -> TX: LIGHTON");
  Serial2.print("LIGHTON\r\n");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLightOff() {
  Serial.println("WEB_REQ -> TX: LIGHTOFF");
  Serial2.print("LIGHTOFF\r\n");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleDoorUnlock() {
  Serial.println("WEB_REQ -> TX: DOORUNLOCK");
  Serial2.print("DOORUNLOCK\r\n");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLockdownOn() {
  Serial.println("WEB_REQ -> TX: LOCKDOWN_ON");
  Serial2.print("LOCKDOWN_ON\r\n");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLockdownOff() {
  Serial.println("WEB_REQ -> TX: LOCKDOWN_OFF");
  Serial2.print("LOCKDOWN_OFF\r\n");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleOccupancyOn() {
  Serial.println("WEB_REQ -> TX: OCCUPANCY_ON");
  Serial2.print("OCCUPANCY_ON\r\n");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleOccupancyOff() {
  Serial.println("WEB_REQ -> TX: OCCUPANCY_OFF");
  Serial2.print("OCCUPANCY_OFF\r\n");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleClimateOn() {
  Serial.println("WEB_REQ -> TX: CLIMATE_ON");
  Serial2.print("CLIMATE_ON\r\n");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleClimateOff() {
  Serial.println("WEB_REQ -> TX: CLIMATE_OFF");
  Serial2.print("CLIMATE_OFF\r\n");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleClearAlarm() {
  Serial.println("WEB_REQ -> TX: CLEARALARM");
  Serial2.print("CLEARALARM\r\n");
  server.sendHeader("Location", "/");
  server.send(303);
}

// ==================== STM32 DATA PARSING ====================
void handleUartLine(String line) {
  line.trim();

  if (line.startsWith("T=")) {
    float t = latestTemp;
    float h = latestHum;
    float g = latestGas;
    int m = latestMotion ? 1 : 0;
    int e = latestEmergency ? 1 : 0;
    int l = latestLockdown ? 1 : 0;
    int o = latestOccupancy ? 1 : 0;
    int c = latestClimate ? 1 : 0;
    int start = 0;

    while (start >= 0) {
      int comma = line.indexOf(',', start);
      String token = (comma == -1) ? line.substring(start) : line.substring(start, comma);
      token.trim();

      if (token.startsWith("T=")) {
        t = token.substring(2).toFloat();
      } else if (token.startsWith("H=")) {
        h = token.substring(2).toFloat();
      } else if (token.startsWith("M=")) {
        m = token.substring(2).toInt();
      } else if (token.startsWith("G=")) {
        g = token.substring(2).toFloat();
      } else if (token.startsWith("E=")) {
        e = token.substring(2).toInt();
      } else if (token.startsWith("L=")) {
        l = token.substring(2).toInt();
      } else if (token.startsWith("O=")) {
        o = token.substring(2).toInt();
      } else if (token.startsWith("C=")) {
        c = token.substring(2).toInt();
      }

      if (comma == -1) {
        break;
      }
      start = comma + 1;
    }

    latestTemp = t;
    latestHum = h;
    latestMotion = (m != 0);
    latestGas = g;
    latestEmergency = (e != 0);
    latestLockdown = (l != 0);
    latestOccupancy = (o != 0);
    latestClimate = (c != 0);
    latestMode = latestEmergency ? "EMERG" : (latestLockdown ? "LOCK" : ((latestOccupancy || latestClimate) ? "AUTO" : "MAN"));
    hasData = true;
  } else if (line.startsWith("ALARM=")) {
    String alarm = line.substring(6);
    alarm.trim();
    if (alarm == "CLEARED") {
      latestAlarm = "NORMAL";
      latestEmergency = false;
    } else {
      latestAlarm = alarm;
      latestEmergency = true;
    }

    if ((alarm == "FIRE") || (alarm == "GAS")) {
      if (isAuthorized && storedChatId != "") {
        bot.sendMessage(storedChatId, "🚨 EMERGENCY: " + alarm, "");
      }
    } else if (alarm == "INTRUSION") {
      if (isAuthorized && storedChatId != "") {
        bot.sendMessage(storedChatId, "🚨 ALERT: Intrusion detected!", "");
      }
    }
  } else if (line == "LOCKED") {
    latestDoorState = "LOCKED";
    Serial.println("STM32_ACK: DOOR LOCKED SUCCESS");
  } else if (line == "UNLOCKED") {
    latestDoorState = "UNLOCKED";
    Serial.println("STM32_ACK: DOOR UNLOCKED SUCCESS");
  } else if (line == "PIR=MOTION") {
    if (isAuthorized && storedChatId != "") {
      bot.sendMessage(storedChatId, "🚨 ALERT: Motion detected!", "");
    }
  }
}

static String normalizeCommand(String text) {
  text.trim();
  text.toLowerCase();
  int atPos = text.indexOf('@');
  if (atPos > 0) text = text.substring(0, atPos);
  return text;
}

// ==================== TELEGRAM HANDLER ====================
void handleTelegramMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String message_id = String(bot.messages[i].message_id);
    
    Serial.println("TG_MSG from [" + chat_id + "]: " + text);

    if (!isAuthorized) {
      if (text == SECRET_PASSPHRASE) {
        bot.sendGetToTelegram("deleteMessage?chat_id=" + chat_id + "&message_id=" + message_id);
        storedChatId = chat_id;
        isAuthorized = true;
        bot.sendMessage(chat_id, "✅ Identity Verified. Commands: /lock, /unlock, /fanon, /fanoff, /lighton, /lightoff, /doorunlock, /lockdownon, /lockdownoff, /occupancyon, /occupancyoff, /climateon, /climateoff, /clearalarm, /data", "");
      } else {
        bot.sendMessage(chat_id, "🔒 System Locked. Send Passphrase.", "");
      }
      continue;
    }

    text = normalizeCommand(text);
    if (text == "/data" || text == "/status") {
      bot.sendMessage(chat_id,
                      "📊 Status:\nTemp: " + String(latestTemp, 1) +
                      "°C\nHum: " + String(latestHum, 1) +
                      "%\nGas: " + String(latestGas, 0) +
                      "\nMotion: " + String(latestMotion ? "YES" : "NO") +
                      "\nMode: " + latestMode +
                      "\nAlarm: " + latestAlarm +
                      "\nDoor: " + latestDoorState,
                      "");
    } 
    else if (text == "/lock") { 
      Serial.println("TG_REQ -> TX: LOCK");
      Serial2.print("LOCK\r\n"); 
      bot.sendMessage(chat_id, "🔒 Lock command sent.", ""); 
    } 
    else if (text == "/unlock") { 
      Serial.println("TG_REQ -> TX: UNLOCK");
      Serial2.print("UNLOCK\r\n"); 
      bot.sendMessage(chat_id, "🔓 Unlock command sent.", ""); 
    } 
    else if (text == "/fanon") { 
      Serial.println("TG_REQ -> TX: FANON");
      Serial2.print("FANON\r\n"); 
      bot.sendMessage(chat_id, "🌬️ Fan ON command sent.", ""); 
    } 
    else if (text == "/fanoff") { 
      Serial.println("TG_REQ -> TX: FANOFF");
      Serial2.print("FANOFF\r\n"); 
      bot.sendMessage(chat_id, "🛑 Fan OFF command sent.", ""); 
    }
    else if (text == "/lighton") { 
      Serial.println("TG_REQ -> TX: LIGHTON");
      Serial2.print("LIGHTON\r\n"); 
      bot.sendMessage(chat_id, "💡 Light ON command sent.", ""); 
    }
    else if (text == "/lightoff") { 
      Serial.println("TG_REQ -> TX: LIGHTOFF");
      Serial2.print("LIGHTOFF\r\n"); 
      bot.sendMessage(chat_id, "💡 Light OFF command sent.", ""); 
    }
    else if (text == "/doorunlock") { 
      Serial.println("TG_REQ -> TX: DOORUNLOCK");
      Serial2.print("DOORUNLOCK\r\n"); 
      bot.sendMessage(chat_id, "🚪 Door unlock pulse sent.", ""); 
    } 
    else if (text == "/lockdownon") {
      Serial.println("TG_REQ -> TX: LOCKDOWN_ON");
      Serial2.print("LOCKDOWN_ON\r\n");
      bot.sendMessage(chat_id, "🔒 Lockdown enabled.", "");
    }
    else if (text == "/lockdownoff") {
      Serial.println("TG_REQ -> TX: LOCKDOWN_OFF");
      Serial2.print("LOCKDOWN_OFF\r\n");
      bot.sendMessage(chat_id, "🔓 Lockdown disabled.", "");
    }
    else if (text == "/occupancyon") {
      Serial.println("TG_REQ -> TX: OCCUPANCY_ON");
      Serial2.print("OCCUPANCY_ON\r\n");
      bot.sendMessage(chat_id, "🏠 Occupancy auto-light enabled.", "");
    }
    else if (text == "/occupancyoff") {
      Serial.println("TG_REQ -> TX: OCCUPANCY_OFF");
      Serial2.print("OCCUPANCY_OFF\r\n");
      bot.sendMessage(chat_id, "🏠 Occupancy auto-light disabled.", "");
    }
    else if (text == "/climateon") {
      Serial.println("TG_REQ -> TX: CLIMATE_ON");
      Serial2.print("CLIMATE_ON\r\n");
      bot.sendMessage(chat_id, "🌡️ Climate auto-fan enabled.", "");
    }
    else if (text == "/climateoff") {
      Serial.println("TG_REQ -> TX: CLIMATE_OFF");
      Serial2.print("CLIMATE_OFF\r\n");
      bot.sendMessage(chat_id, "🌡️ Climate auto-fan disabled.", "");
    }
    else if (text == "/clearalarm") {
      Serial.println("TG_REQ -> TX: CLEARALARM");
      Serial2.print("CLEARALARM\r\n");
      bot.sendMessage(chat_id, "✅ Clear alarm requested.", "");
    }


    else if (text == "/logout") { 
      isAuthorized = false; 
      storedChatId = ""; 
      bot.sendMessage(chat_id, "🚪 Logged out.", ""); 
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
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP()); 

  secureClient.setInsecure();

  // Web Server Routes
  server.on("/", handleRoot);
  server.on("/lock", handleLock);
  server.on("/unlock", handleUnlock);
  server.on("/fanon", handleFanOn);
  server.on("/fanoff", handleFanOff);
  server.on("/lighton", handleLightOn);
  server.on("/lightoff", handleLightOff);
  server.on("/doorunlock", handleDoorUnlock);
  server.on("/lockdownon", handleLockdownOn);
  server.on("/lockdownoff", handleLockdownOff);
  server.on("/occupancyon", handleOccupancyOn);
  server.on("/occupancyoff", handleOccupancyOff);
  server.on("/climateon", handleClimateOn);
  server.on("/climateoff", handleClimateOff);
  server.on("/clearalarm", handleClearAlarm);
  server.begin();
  Serial.println("WEB SERVER: Online");
}

void loop() {
  server.handleClient(); 

  while (Serial2.available()) {
    char c = Serial2.read();
    if ((uint8_t)c == 0xFF) {
      latestAlarm = "EMERGENCY";
      latestEmergency = true;
      if (isAuthorized && storedChatId != "") {
        bot.sendMessage(storedChatId, "🚨 EMERGENCY: Sensor trip detected!", "");
      }
      continue;
    }
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