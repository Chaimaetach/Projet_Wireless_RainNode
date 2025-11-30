#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <math.h>
#include <time.h>

// ==== Wi-Fi / MQTT over TLS (MQTTS) ====
const char* WIFI_SSID = "iPhone Chaimae 2";
const char* WIFI_PASS = "chaimae2002";

// same EMQX server as your ESP32
const char* MQTT_SERVER = "ybb21bfa.ala.us-east-1.emqxsl.com";
const int   MQTT_PORT   = 8883;

// same username/password as ESP32
const char* MQTT_USER   = "esp32user";
const char* MQTT_PASS   = "esp32pass123";

const char* TOPIC_RAIN_INT   = "maison/jardin/pluie/intensite";
const char* TOPIC_RAIN_STATE = "maison/jardin/pluie/etat";

// ==== Root CA (same as ESP32) ====
static const char *root_ca = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)EOF";

// ==== TLS CLIENT ====
WiFiClientSecure espClient;
PubSubClient client(espClient);

// ---- état ----
float  g_rainPercent = -1.0f;
String g_state = "-";
uint32_t g_lastUpdateMs = 0;
bool g_mqttOk = false;

// ---- couleurs ----
#define C_BG      TFT_BLACK
#define C_TEXT    TFT_WHITE
#define C_MUTED   0x7BEF
#define C_CARD    0x0021
#define C_CYAN    TFT_CYAN
#define C_GREEN   TFT_GREEN
#define C_RED     TFT_RED

M5Canvas canvas(&M5.Lcd);

// --- helper NTP pour TLS ---
void waitForTime(uint32_t timeout_ms = 10000) {
  time_t now = 0;
  uint32_t start = millis();
  do {
    time(&now);
    if (now > 1700000000) break; // ~2023+
    delay(200);
  } while (millis() - start < timeout_ms);
}

// ---- UI helpers ----
void pill(int x, int y, int w, int h, uint16_t stroke, uint16_t fill, const char* txt, uint16_t txtcol) {
  canvas.fillRoundRect(x, y, w, h, h/2, fill);
  canvas.drawRoundRect(x, y, w, h, h/2, stroke);
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(txtcol, fill);
  canvas.setTextSize(2);
  canvas.drawString(txt, x + w/2, y + h/2 + 1);
}

void drawArcGauge(int cx, int cy, int r, float percent, uint16_t col) {
  canvas.drawCircle(cx, cy, r, C_MUTED);
  canvas.drawCircle(cx, cy, r-1, C_MUTED);

  float amax = constrain(percent, 0, 100) * 300.0f / 100.0f;
  float start = 120.0f;
  for (float a = 0; a <= amax; a += 1.5f) {
    float rad = (start + a) * DEG_TO_RAD;
    int x = cx + cosf(rad) * (r-2);
    int y = cy + sinf(rad) * (r-2);
    canvas.drawPixel(x, y, col);
    canvas.drawPixel(x+1, y, col);
    canvas.drawPixel(x, y+1, col);
  }

  canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(C_TEXT, C_BG);
  canvas.setTextSize(6);

  if (percent < 0) canvas.drawString("--", cx, cy-6);
  else {
    char b[8]; snprintf(b, sizeof(b), "%.0f", percent);
    canvas.drawString(b, cx, cy-6);
  }

  canvas.setTextSize(2);
  canvas.setTextColor(C_MUTED, C_BG);
  canvas.drawString("Intensite (%)", cx, cy + 34);
}

void drawScreen() {
  int W = M5.Lcd.width();
  int H = M5.Lcd.height();

  if (canvas.width() == 0) {
    canvas.setColorDepth(16);
    canvas.createSprite(W, H);
  }

  canvas.fillScreen(C_BG);

  const int PAD = 18;
  const int headerH = 48;
  const int footerH = 28;

  // HEADER
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextColor(C_TEXT, C_BG);
  canvas.setTextSize(2);
  canvas.setCursor(PAD, PAD-2);
  canvas.println("Station Pluie • MQTT");

  // Badges Wi-Fi / MQTT
  int bx = W - PAD - 120;
  pill(bx, PAD-6, 120, 26, C_MUTED, C_BG,
       WiFi.isConnected() ? "Wi-Fi: OK" : "Wi-Fi: OFF",
       WiFi.isConnected() ? C_GREEN : C_RED);

  bx -= 126;
  pill(bx, PAD-6, 120, 26, C_MUTED, C_BG,
       g_mqttOk ? "MQTT: OK" : "MQTT: OFF",
       g_mqttOk ? C_GREEN : C_RED);

  // CONTENU
  int contentY = headerH + PAD;
  int contentH = H - contentY - footerH - PAD;
  int contentW = W - PAD*2;

  canvas.fillRoundRect(PAD, contentY, contentW, contentH, 12, C_CARD);
  canvas.drawRoundRect(PAD, contentY, contentW, contentH, 12, C_MUTED);

  int cx = PAD + contentW/2;
  int cy = contentY + contentH/2 - 4;
  int r  = min(contentW, contentH) / 4 + 10;

  uint16_t gaugeCol = (g_state=="RAIN") ? C_GREEN : (g_state=="DRY" ? C_CYAN : C_MUTED);
  drawArcGauge(cx, cy, r, g_rainPercent, gaugeCol);

  uint16_t stCol = (g_state=="RAIN") ? C_GREEN : (g_state=="DRY") ? C_CYAN : C_TEXT;
  int pillW = 190, pillH = 42;
  pill(cx - pillW/2, cy + r + 16, pillW, pillH,
       stCol, C_BG, ("Etat : " + g_state).c_str(), stCol);

  // FOOTER
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextColor(C_MUTED, C_BG);
  canvas.setTextSize(2);
  canvas.setCursor(PAD, H - footerH);

  if (WiFi.isConnected())
    canvas.printf("IP %s  ", WiFi.localIP().toString().c_str());

  if (g_lastUpdateMs == 0) canvas.print("en attente de donnees...");
  else {
    uint32_t s = (millis() - g_lastUpdateMs) / 1000;
    canvas.printf("maj il y a %lus", (unsigned long)s);
  }

  canvas.pushSprite(0, 0);
}

// ================= MQTT =================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String t = topic;
  String msg; msg.reserve(length);
  for (unsigned int i = 0; i < length; ++i) msg += (char)payload[i];

  if (t == TOPIC_RAIN_INT) {
    g_rainPercent = msg.toFloat();
    g_lastUpdateMs = millis();
  } else if (t == TOPIC_RAIN_STATE) {
    g_state = msg;
    g_lastUpdateMs = millis();
  }

  drawScreen();
}

void ensureMqtt() {
  if (!WiFi.isConnected()) { g_mqttOk = false; return; }
  if (client.connected()) return;

  // TLS + username/password
  if (client.connect("M5Tab5_Display_UI_TLS", MQTT_USER, MQTT_PASS)) {
    client.subscribe(TOPIC_RAIN_INT);
    client.subscribe(TOPIC_RAIN_STATE);
    g_mqttOk = true;
  } else {
    g_mqttOk = false;
  }
}

// ================= Setup / Loop =================
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Lcd.setRotation(1);
  M5.Lcd.setBrightness(230);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) delay(200);

  // TLS : NTP + CA
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  waitForTime();
  espClient.setCACert(root_ca);

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqttCallback);
  client.setKeepAlive(10);
  client.setBufferSize(256);

  drawScreen();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
  ensureMqtt();
  client.loop();

  static uint32_t t = 0;
  if (millis() - t > 1000) { t = millis(); drawScreen(); }
}
