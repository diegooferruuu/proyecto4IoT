#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

class TemperatureSensor {
private:
    OneWire oneWire;
    DallasTemperature sensors;
    int sensorPin;
    int currentState;

public:
    TemperatureSensor(int pin) : sensorPin(pin), oneWire(pin), sensors(&oneWire), currentState(-1) {}

    void setup() {
        sensors.begin();
    }

    float readTemperature() {
        sensors.requestTemperatures();
        float temperatureC = sensors.getTempCByIndex(0);

        if (temperatureC == DEVICE_DISCONNECTED_C) {
            Serial.println("Error: Unable to read temperature.");
            return DEVICE_DISCONNECTED_C;
        }

        Serial.print("Temperature: ");
        Serial.print(temperatureC);
        Serial.println(" °C");
        return temperatureC;
    }

    int getState(float temperature) {
        if (temperature >= -55 && temperature < 35) return 0;
        if (temperature >= 35 && temperature < 40.9) return 1;
        if (temperature >= 40.5 && temperature < 125) return 2;
        return -1; 
    }

    bool updateState(float temperature) {
        int newState = getState(temperature);
        if (newState != currentState) {
            currentState = newState;
            return true;
        }
        return false;
    }

    int getCurrentState() {
        return currentState;
    }

    void setState(int state) {
      currentState = state;
    }
};

class Communicator {
private:
    const char* ssid;
    const char* password;
    const char* mqtt_server;
    WiFiClientSecure espClient;
    PubSubClient client;
    const char* topic;
    bool autoMode;
    bool needsForcePublish;
    int lastKnownState;

public:
    Communicator(const char* ssid, const char* password, const char* mqtt_server, const char* topic)
        : ssid(ssid), password(password), mqtt_server(mqtt_server), topic(topic), client(espClient), autoMode(false) {}

    void setup() {
        espClient.setCACert(aws_root_ca_cert);
        espClient.setCertificate(client_cert);
        espClient.setPrivateKey(client_key);
        setupWifi();
        client.setServer(mqtt_server, 8883);
        client.setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->callback(topic, payload, length);
        });
    }

    void loop() {
        if (!client.connected()) {
            reconnect();
        }
        client.loop();
    }

    void publishForce() {
        Serial.println("Forcing publication of temperature state");
        StaticJsonDocument<200> doc;
        doc["state"]["reported"]["temperatureState"] = lastKnownState;
        doc["state"]["reported"]["incubatorMode"] = autoMode ? 1 : 0;

        char buffer[256];
        serializeJson(doc, buffer);

        if (client.publish(topic, buffer)) {
            Serial.println("Force publish: Shadow updated successfully");
        } else {
            Serial.println("Force publish: Error updating shadow");
        }
    }

    void publishTemperatureState(int tempState) {
        lastKnownState = tempState; 
        
        StaticJsonDocument<200> doc;
        doc["state"]["reported"]["temperatureState"] = tempState;
        doc["state"]["reported"]["incubatorMode"] = autoMode ? 1 : 0; 

        char buffer[256];
        serializeJson(doc, buffer);

        if (client.publish(topic, buffer)) {
            Serial.println("Shadow updated successfully");
        } else {
            Serial.println("Error updating shadow");
        }
    }

    void setAutoMode(bool autoEnabled) {
        autoMode = autoEnabled;
        Serial.print("Auto mode set to: ");
        Serial.println(autoEnabled ? "ON" : "OFF");

        if (!autoEnabled) {
            needsForcePublish = true;
            Serial.println("Force publish flag activated");
        } else if (needsForcePublish) {
            publishForce();
            needsForcePublish = false;
            Serial.println("Force publish flag reset");
        }
    }

    bool isAutoMode() const {
        return autoMode;
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

        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    }

    void reconnect() {
        while (!client.connected()) {
            Serial.print("Connecting to MQTT broker...");
            String clientId = "ESP32Client-";
            clientId += String(random(0xffff), HEX);
            if (client.connect(clientId.c_str())) {
                Serial.println("connected");
                client.subscribe("$aws/things/incubator_v2/shadow/update/accepted");
            } else {
                Serial.print("failed, rc=");
                Serial.print(client.state());
                Serial.println(" trying again in 5 seconds");
                delay(5000);
            }
        }
    }

    void callback(char* topic, byte* payload, unsigned int length) {
        StaticJsonDocument<200> doc;
        deserializeJson(doc, payload, length);

        if (String(topic) == "$aws/things/incubator_v2/shadow/update/accepted") {
            if (doc["state"]["desired"].containsKey("incubatorMode")) {
                bool newAutoMode = doc["state"]["desired"]["incubatorMode"] == 1;
                setAutoMode(newAutoMode);
            }
        }
        else if (doc["state"].containsKey("incubatorMode")) {
            setAutoMode(doc["state"]["incubatorMode"] == 1);
            Serial.println("Auto mode updated from shadow delta.");
        }
    }

    const char* aws_root_ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)EOF";

    const char* client_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDWjCCAkKgAwIBAgIVAO+sxQb7eVAXr2Ycv5cfGN77WgmKMA0GCSqGSIb3DQEB
CwUAME0xSzBJBgNVBAsMQkFtYXpvbiBXZWIgU2VydmljZXMgTz1BbWF6b24uY29t
IEluYy4gTD1TZWF0dGxlIFNUPVdhc2hpbmd0b24gQz1VUzAeFw0yNDExMjgxNjUx
NDZaFw00OTEyMzEyMzU5NTlaMB4xHDAaBgNVBAMME0FXUyBJb1QgQ2VydGlmaWNh
dGUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDfwuIw8c6/6Cv8IzCE
z2fTVfkyEFZm9PJX68nl3VOmPcEXN9NQQFxtnwcYo/lcj7L3FGrj//cS/aYhQbCI
qxJoK3aHF6uNV7jiMqRTYaSMq0VM/T2gwXh9bsDnvG47EnKxxYVZfq6jObHxQLiD
/X4sohBNTxBtMO7/Z3xMIMpZCC0uNHYeggLDC/Sox7msNqchsjmScxHWa6AvsM9k
XfYXAYxTAIDwjN8m4r7yFawwqjh6fDQMxUCgPFFINPhgke7t6zcvJEO7y6JcQkaf
/yCcrI87kYvnIcUGWydI9igG1X+ooLH6R7ZwtrYU43KLKrnc8ZyB5wlRWJG6dG52
ag4zAgMBAAGjYDBeMB8GA1UdIwQYMBaAFK61ygEJBknwAI9Z90g+3uvL6R+8MB0G
A1UdDgQWBBROlymTBJsLLYeRt2OBgxZnA64cGzAMBgNVHRMBAf8EAjAAMA4GA1Ud
DwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAp1+VDT2MM83sXG12p7hnNZSZ
Vhy+wT1KmjQYjRetTF+jTwU6Bo23emP/UOyTbhhRSN9QV4ryDzh7qC2LJIPdDDpO
orGmb8+WhN8P3qUuVkts0Z/98EHZeSnZtgrhzTgMchvod2cx8EJ00uhBOADuq8nI
X73fFhHAkSjm4dBxLe8UjWPsvehIZLqgcUDFM1Alo0hm2rGe4EWCKdvhOGLISBb5
hZvj+j5V9swiQfseBvvdGFz6s8Y9eiCn46zR/RntlBZbqWhuVfdihPqX+pig26A6
5DdJAloeFmP5v+XBNkcNfrLahNwpkEjtliSA5ZxnRdkzJHM0fvbsm8ZraZhEPA==
-----END CERTIFICATE-----
)EOF";

    const char* client_key = R"EOF(
-----BEGIN RSA PRIVATE KEY-----
MIIEpQIBAAKCAQEA38LiMPHOv+gr/CMwhM9n01X5MhBWZvTyV+vJ5d1Tpj3BFzfT
UEBcbZ8HGKP5XI+y9xRq4//3Ev2mIUGwiKsSaCt2hxerjVe44jKkU2GkjKtFTP09
oMF4fW7A57xuOxJyscWFWX6uozmx8UC4g/1+LKIQTU8QbTDu/2d8TCDKWQgtLjR2
HoICwwv0qMe5rDanIbI5knMR1mugL7DPZF32FwGMUwCA8IzfJuK+8hWsMKo4enw0
DMVAoDxRSDT4YJHu7es3LyRDu8uiXEJGn/8gnKyPO5GL5yHFBlsnSPYoBtV/qKCx
+ke2cLa2FONyiyq53PGcgecJUViRunRudmoOMwIDAQABAoIBAQDRLAzmVCVBoHf9
q6eff4M/PA9HjZ43gKNLLU//YXbg4LmwSq9vRbshfpfToPVqYaqa7qs9l/oP9q1m
SVINcF40ouyaPLZR2r8qNM0IZS69kR2natRBgppwJyiNeMhS0FwHcu3FS3dVfomp
TScvu7hHYPh8THy+lg/VEZhQUhbWQd78R9fgnnroq0ZKfHJgHdT0YfL31lTcYxyq
x3ciJNj6WHA0uaE9/8kI51VJn0y61mgr769T/9h3mvQp/grD/sn8JPFCIYuHH7Bk
HJDbzFalE/kuTnZ+XwCObDlcl8K1IFHe+RBeb/AAhvitZA+b5EDGkLmdQ6+mo912
uTb7XFsBAoGBAPGHDbyqqGxKENXbyH9x+bxsXNo707+/x3nC4oNjWIU1gixl2IbF
XIbHZqQER10G+rwRYpD8/5FKTiVkskbcznRMWa7WA73re1GGRhNQ8O2mSwE3SjsF
AWxhKsEkvtdDJWn4yEVx8sY3f+oaBjKDf1k3vMO3rBbU9tw1sjK2/+arAoGBAO0r
TSaHnQhvmE7qIr4E83w/1bWchGP+vVFbVq36WiuvnYzvDc+MVFo1bfBWFeNHOUSk
GmRmbILG6AnFNuxXZY09JueXbVpAuOrce62f1PVQMbEKduYfvyOSFMEIIYXLw9hy
n91bmjXBCm5tVs+THpaOABhQSs40oM5YeH+W85aZAoGBANu5loit5JznYJt938/5
e02sc2hiBwgpSyp9Xh5TccN141/Ko4ov4DYJD1plPhrmb5JKapYmY8XkiGVCT2Fi
HTv1/ZytvCe6I60N75BBEX3Ry+hmSVSaACp91yxw5mDlCBN5LZDn0gn7kZYCaOLp
eCBNMaAH5zm6Mn4xJ1U14GYTAoGBAJH2il7iK6UbeTNoKcWkqm2Xo+FH2Lrp9n3x
puEgKQql5GLuZSONDAUQyP6XHJVqKjaygIO1/aZm4DvTIkBGcXuBXUY/Dc/z8Vqq
I3vx2FC58DszY4WxmCM+LoO2i1sa0weNVBIZ5Eic69yWck0CEFs8PZa5I7DQ9v7H
uDFeJ8BRAoGAeDsGBF/M1DnpWiwMDznegyQM4uIpblY07Vdix+Qe3CEx2roHh1dZ
qcYnIzsfUgAxzMXR5R8X7gE0fS1AaWNzfr+eXeO8Wma1asaVGjjVmAq14ExPRXwm
d7MypAUpsGvQdm+TegCU5XucawZXhhNuhcTFOaPhv02WvG6ht/CftOw=
-----END RSA PRIVATE KEY-----
)EOF";
};

TemperatureSensor temperatureSensor(4);
Communicator comm("FERRUVEGA", "Ignacio73*-", "a1o8cg6i3hlsiy-ats.iot.us-east-2.amazonaws.com", "$aws/things/incubator_v2/shadow/update");

void setup() {
    Serial.begin(115200);
    temperatureSensor.setup();
    comm.setup();
}

void loop() {
    comm.loop();

    float temperature = temperatureSensor.readTemperature();
    if (temperatureSensor.updateState(temperature)) {
        comm.publishTemperatureState(temperatureSensor.getCurrentState());
    }

    delay(2000);
}
