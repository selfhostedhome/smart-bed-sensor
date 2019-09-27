#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <HX711.h>
#include <PubSubClient.h>

#include "config.h"

WiFiClient wifiClient;
PubSubClient client(wifiClient);

HX711 scale;
const int HX711_DT;
const int HX711_SCK;

void reconnect() {
    // Loop until we're reconnected
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect(HOSTNAME)) {
            Serial.println("connected");
            // Once connected, publish an announcement...
            client.publish(AVAILABILITY_TOPIC, "online");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void setupOTA() {
    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.setHostname(HOSTNAME);

    ArduinoOTA.onStart([]() { Serial.println("Starting"); });
    ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            Serial.println("End Failed");
        }
    });
    ArduinoOTA.begin();
}

void setup() {
    Serial.begin(115200);
    Serial.println();

    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASSWORD);
    Serial.print("Connecting...");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());

    client.setServer(MQTT_SERVER, 1883);

    setupOTA();

    // Setting up heartbeat
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    static int lastValue = 0;
    int value;

    if (!client.connected()) {
        reconnect();
    }

    value = scale.read_average(5);
    Serial.println(value);

    // Only publish new value if difference of 500 or more
    if (abs(lastValue - value) >= 500) {
        String value_str = String(value);
        client.publish(STATE_TOPIC, (char *)value_str.c_str());
        lastValue = value;
    }

    // Handle OTA
    ArduinoOTA.handle();

    // Process MQTT tasks
    client.loop();

    // Heartbeat
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(250);

    scale.power_down(); // put the ADC in sleep mode
    delay(1000);
    scale.power_up();
}
