#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// AWS Certificates
const char AWS_CERT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
-----END CERTIFICATE-----
)EOF";

const char AWS_CERT_CRT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
-----END CERTIFICATE-----
)EOF";

const char AWS_CERT_PRIVATE[] PROGMEM = R"EOF(
-----BEGIN RSA PRIVATE KEY-----
-----END RSA PRIVATE KEY-----
)EOF";

class IncandescentLight {
private:
    int lightPin;

public:
    IncandescentLight(int pin) : lightPin(pin) {}

    void setup() {
        pinMode(lightPin, OUTPUT);
        digitalWrite(lightPin, LOW);
    }

    void setState(int state) {
        if (state == 1) {
            digitalWrite(lightPin, HIGH);
            Serial.println("Light ON");
        } else {
            digitalWrite(lightPin, LOW);
            Serial.println("Light OFF");
        }
    }
};

class Cooler {
private:
    int coolerPin;

public:
    Cooler(int pin) : coolerPin(pin) {}

    void setup() {
        pinMode(coolerPin, OUTPUT);
        digitalWrite(coolerPin, LOW);
    }

    void setState(int state) {
        if (state == 1) {
            digitalWrite(coolerPin, HIGH);
            Serial.println("Cooler ON");
        } else {
            digitalWrite(coolerPin, LOW);
            Serial.println("Cooler OFF");
        }
    }
};

class Communicator {
private:
    const char* ssid;
    const char* password;
    const char* mqtt_server;
    const char* mqtt_topic;
    WiFiClientSecure espClient;
    PubSubClient client;
    IncandescentLight& light;
    Cooler& cooler;

public:
    Communicator(const char* ssid, const char* password, const char* mqtt_server, const char* mqtt_topic, IncandescentLight& light, Cooler& cooler)
        : ssid(ssid), password(password), mqtt_server(mqtt_server), mqtt_topic(mqtt_topic), client(espClient), light(light), cooler(cooler) {}

    void setup() {
        setupWifi();
        setupMQTT();
    }

    void loop() {
        if (!client.connected()) {
            reconnect();
        }
        client.loop();
    }

private:
    void setupWifi() {
        delay(10);
        Serial.println();
        Serial.print("Connecting to ");
        Serial.println(ssid);

        WiFi.begin(ssid, password);

        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }

        Serial.println("\nWiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    }

    void setupMQTT() {
        espClient.setCACert(AWS_CERT_CA);
        espClient.setCertificate(AWS_CERT_CRT);
        espClient.setPrivateKey(AWS_CERT_PRIVATE);
        client.setServer(mqtt_server, 8883);
        client.setCallback([this](char* topic, byte* payload, unsigned int length) { this->callback(topic, payload, length); });
    }

    void callback(char* topic, byte* payload, unsigned int length) {
        char jsonBuffer[512];
        if (length >= sizeof(jsonBuffer)) {
            Serial.println("Payload too large");
            return;
        }
        memcpy(jsonBuffer, payload, length);
        jsonBuffer[length] = '\0';

        Serial.print("Received payload: ");
        Serial.println(jsonBuffer);

        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, jsonBuffer);

        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.f_str());
            return;
        }

        JsonObject state = doc["state"];
        if (!state.isNull()) {
            JsonObject desired = state["desired"];
            if (!desired.isNull()) {
                if (desired.containsKey("lightState")) {
                    int lightState = desired["lightState"];
                    light.setState(lightState);
                }
                if (desired.containsKey("coolerState")) {
                    int coolerState = desired["coolerState"];
                    cooler.setState(coolerState);
                }
            } else {
                Serial.println("desired object is null");
            }
        } else {
            Serial.println("state object is null");
        }
    }

    void reconnect() {
        while (!client.connected()) {
            Serial.print("Attempting MQTT connection...");
            String clientId = "ESP32Client-";
            clientId += String(random(0xffff), HEX);

            if (client.connect(clientId.c_str())) {
                Serial.println("connected");
                client.subscribe(mqtt_topic);
            } else {
                Serial.print("failed, rc=");
                Serial.print(client.state());
                Serial.println(" try again in 5 seconds");
                delay(5000);
            }
        }
    }
};

// Pins for light and cooler
IncandescentLight light(5); // Replace with your light pin
Cooler cooler(25);           // Replace with your cooler pin

// Wi-Fi and MQTT configurations
Communicator communicator(
    "Galaxy A20s3847", "camilo2003",
    "a1o8cg6i3hlsiy-ats.iot.us-east-2.amazonaws.com",
    "$aws/things/tempEsp32/shadow/update/accepted",
    light, cooler);

void setup() {
    Serial.begin(115200);
    light.setup();
    cooler.setup();
    communicator.setup();
}

void loop() {
    communicator.loop();
}
