#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

// ==== Wi-Fi ====
const char* WIFI_SSID = "iPhone Chaimae 2";
const char* WIFI_PASS = "chaimae2002";

// ==== MQTT over TLS (MQTTS) ====
// put here your EMQX host + TLS port 8883
const char* MQTT_SERVER = "ybb21bfa.ala.us-east-1.emqxsl.com";   // <--- CHANGE IF NEEDED
const int   MQTT_PORT   = 8883;

// put here the username/password you created in EMQX
const char* MQTT_USER   = "esp32user";
const char* MQTT_PASS   = "esp32pass123";

const char* TOPIC_RAIN_INT   = "maison/jardin/pluie/intensite";
const char* TOPIC_RAIN_STATE = "maison/jardin/pluie/etat";

// ==== Root CA (paste EMQX CA here) ====
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

// ==== Broches capteur pluie ====
#define RAIN_AO_PIN  34
#define RAIN_DO_PIN  26

const float RAIN_THRESHOLD_PERCENT = 5.0;

// ==== Client TLS ====
WiFiClientSecure espClient;
PubSubClient client(espClient);

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

// --- Connexion MQTT ---
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connexion au broker MQTT TLS...");
    // ID client + user/pass
    if (client.connect("ESP32_RainSensor_TLS", MQTT_USER, MQTT_PASS)) {
      Serial.println(" OK");
    } else {
      Serial.print(" echec, code = ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RAIN_AO_PIN, INPUT);
  pinMode(RAIN_DO_PIN, INPUT);

  // Wi-Fi
  Serial.print("Connexion au Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connecte !");
  Serial.print("IP : ");
  Serial.println(WiFi.localIP());

  // TLS : heure + CA
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  waitForTime();
  espClient.setCACert(root_ca);   // vérifie le certificat serveur

  // MQTT TLS
  client.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
  if (!client.connected()) reconnectMQTT();
  client.loop();

  int raw = analogRead(RAIN_AO_PIN);

  float rainPercent = (4095 - raw) * 100.0 / 4095.0;
  if (rainPercent < 0)   rainPercent = 0;
  if (rainPercent > 100) rainPercent = 100;

  String etat = (rainPercent < RAIN_THRESHOLD_PERCENT) ? "DRY" : "RAIN";

  int doState = digitalRead(RAIN_DO_PIN);

  Serial.print("AO brut : ");
  Serial.print(raw);
  Serial.print("  => Pluie : ");
  Serial.print(rainPercent, 1);
  Serial.println(" %");

  Serial.print("DO (debug) : ");
  Serial.println(doState);

  Serial.print("Etat calcule : ");
  Serial.println(etat);
  Serial.println("-------------------");

  client.publish(TOPIC_RAIN_INT,   String(rainPercent, 1).c_str());
  client.publish(TOPIC_RAIN_STATE, etat.c_str());

  delay(2000);
}
