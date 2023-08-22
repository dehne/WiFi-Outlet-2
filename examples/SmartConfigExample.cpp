#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

void setup() {
    int retry=0, config_done=0;

    WiFi.mode(WIFI_STA);                                // configure WiFi in Station Mode
    Serial.begin(9600);                                 // configure serial port baud rate
    pinMode(16, OUTPUT);                                // configure on-board LED as output pin
    digitalWrite(16, LOW);                              // turn LED on

    // check whether WiFi connection can be established
    Serial.println("Attempt to connect to WiFi network…");
    while(WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
        // If we didn't connect after 10 seconds, try SmartConfig
        if (retry++ >= 20) {
            Serial.println("Connection timeout expired! Start SmartConfig...");
            WiFi.beginSmartConfig();

            // forever loop: exit only when SmartConfig packets have been received
            while (true) {
                delay(500);
                Serial.print(".");
                if (WiFi.smartConfigDone()) {
                    Serial.println("SmartConfig successfully configured");
                    config_done=1;
                    break; // exit from loop
                }
                toggleLED();
            }
            if (config_done==1)
            break;
        }
        }
    // turn LED off
    digitalWrite(16, HIGH);

    // wait for IP address assignment
    while(WiFi.status() != WL_CONNECTED) {
        delay(50);
    }
    // show WiFi connection data
    Serial.println("“”");
    WiFi.printDiag(Serial);

    // show the IP address assigned to our device
    Serial.println(WiFi.localIP());
}

void loop() {
// nothing to do!
}

void toggleLED() {
    static int pinStatus=LOW;

    if (pinStatus==HIGH) {
        pinStatus=LOW;
    } else {
        pinStatus=HIGH;
    }
    digitalWrite(16, pinStatus);
}
