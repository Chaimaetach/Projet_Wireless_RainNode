#include <WiFi.h>
#include <PubSubClient.h>

// ==== Wi-Fi ====
const char* WIFI_SSID = "iPhone Chaimae 2";
const char* WIFI_PASS = "chaimae2002";

// ==== MQTT (non TLS) ====
const char* MQTT_SERVER = "broker.mqtt.cool";
const int   MQTT_PORT   = 1883;
const char* TOPIC_RAIN_INT   = "maison/jardin/pluie/intensite";
const char* TOPIC_RAIN_STATE = "maison/jardin/pluie/etat";

// ==== Broches capteur pluie ====
#define RAIN_AO_PIN  34
#define RAIN_DO_PIN  26

const float RAIN_THRESHOLD_PERCENT = 5.0;

// ==== Client non sécurisé (port 1883) ====
WiFiClient espClient;
PubSubClient client(espClient);

// --- Connexion MQTT ---
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connexion au broker MQTT...");
    if (client.connect("ESP32_RainSensor")) {  // ID client
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

  // MQTT (rien de TLS ici)
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
