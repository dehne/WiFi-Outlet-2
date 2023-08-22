/****
 * @file main.cpp
 * @version 0.1.0
 * 
 * WiFi Outlet -- Replacement firmware for the 2017 Sharper Image model 70011 WiFi 
 * controlled outlet. 
 * 
 * About this project
 * ==================
 * 
 * The goal of the project is to make firmware that presents the device as a web server on the 
 * local WiFi network. It presents web pages that enable it to be set up and controlled by 
 * connecting to it with a web browser. Quite what I want it do, I'm not clear.
 * 
 * As a proof of concept, the home page has a button on it that when clicked toggles the state 
 * of the outlet. The device's LED shows whether the outlet is on or off. The device's button 
 * also toggles the state of the outlet.
 * 
 * The implementation uses -- in addition to all the ESP WiFi stuff -- a super simple web 
 * server I wrote for the purpose. See SimpleWebServer.h for details.
 * 
 * Notes on the hardware
 * =====================
 * 
 * * The TYWE3S daughterboard in this device contains an ESP8266 microprocessor, SPI flash and 
 *   some other stuff I haven't looked at. It exposes some of the ESP8266's pins. Enough to
 *   let us hack the device. The TYWE3S pin layout is as follows:
 * 
 *             <Antenna>
 *          Gnd         Vcc
 *       GPIO15         GPIO13
 *        GPIO2         GPIO12
 *        GPIO0         GPIO14
 *        GPIO4         GPIO16
 *        GPIO5         EN
 *         RXD0         ADC
 *         TXD0         RST
 * 
 * * GPIO0 is connected to one side of the button on the device. The other side is connected to 
 *   Gnd. So, "active LOW."
 * 
 * * GPIO13 is connected to one side of the LED. The other side is connected, via a resistor to 
 *   Vcc. So, the LED is "active LOW."
 * 
 * * GPIO14 is connected to the base of transistor Q1, the driver for the relay that turns the 
 *   outlet on and off. It's "active HIGH." To turn the outlet on, hold GPIO14 HIGH.
 * 
 * * The other GPIOs and ADC aren't hooked to anything, so far as I know.
 * 
 * * To hack the device, you'll need to solder wires to Gnd, Vcc, RXD0, TXD0 and, for convenience, 
 *   to GPIO0 and RST. Connect all but the last two of these to an FTDI serial-to-USB device. 
 *   (Gnd --> GND, Vcc --> 3V3, RXD0 --> TXD, and TXD0 --> RXD). Put a female Dupont connector on 
 *   the GPIO0 wire and a male one on the wire from RST. Find a place on the board connected to 
 *   Gnd and solder a piece of wire to act as a header pin there. (The pads for the unoccupied R24 
 *   and R28 nearest the electrolytic capacitor worked for me.)
 * 
 * * To put the ESP8266 into "PGM from UART" mode, GPIO00 needs to be connected to Gnd. That can 
 *   be done by attaching the wire from GPIO0 to the new header pin. Leaving GPIO0 floating 
 *   results in the ESP8266 entering "Boot from SPI Flash mode." I.e., running normally.
 * 
 * * If the ESP8266 is "soft reset" in "PGM from UART" mode, which is what happens after new firmware 
 *   is loaded into it, it will go into "Boot from SPI Flash" mode, even with GPIO0 attached to Gnd.
 * 
 * * To hardware reset the ESP8266, momentarily connect the wire from RST to Gnd.
 * 
 *****
 * 
 * Copyright (C) 2023 D.L. Ehnebuske
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 * 
 ****/

#include <Arduino.h>                                // The base Arduino framework
#include <ESP8266WiFi.h>                            // The ESP8266 WiFi support
#include <WiFiUdp.h>                                // UDP support needed by SmartCOnfig
#include <ESP_EEPROM.h>                             // Enhanced EEPROM emulator for ESP8266
#include <PushButton.h>                             // My pushbutton support library
#include <UserInput.h>                              // My simple command line support library
#include <SimpleWebServer.h>                        // The web server library in ../lib/SimpleWebServe

// Pin definitions
#define LED             (13)                        // On the WiFi outlet PCB, this is active LOW
#define BUTTON          (0)                         // Active LOW
#define RELAY           (14)                        // On the WiFi outlet PCB, the relay that controls the outlet

// Misc constants
#define BANNER              "WiFi Switch V0.1.0"    // The tagline to identify this sketch
#define TOGGLE_QUERY        "outlet=toggle"         // The URI query string to cause the outlet to toggle state
#define LED_LIT             (LOW)                   // digitalWrite value to light the LED
#define LED_DARK            (HIGH)                  // digitalWrite value to turn the LED off
#define RELAY_OPEN          (LOW)                   // digitlWrite value to open the relay
#define RELAY_CLOSED        (HIGH)                  // digitalWrite value to close the relay
#define SERIAL_CONN_MILLIS  (4000)                  // millis() to wait after Serial.begin() to start using it
#define WIFI_DELAY_MILLIS   (500)                   // millis() to delay between printing "." while waitinf for WiFi
#define WIFI_CONN_MILLIS    (15000)                 // millis() to wait for WiFi connect before giving up
#define CONFIG_SIG          (0x3840)                // Our "signature" in EEPROM to know the data is (probably) ours

struct eepromData_t {
    uint16_t signature;
    char ssid[33];
    char password[64];
};

WiFiServer wiFiServer {80};
SimpleWebServer webServer;
PushButton button {BUTTON};
UserInput ui {};
eepromData_t config {0, "", ""};
bool running;

// The HTML for the device's "home page"
const char* homePage  = "<!DOCTYPE html>\n"
                        "<html>\n"
                        "  <head>\n"
                        "    <meta charset=\"utf-8\" />\n"
                        "  </head>\n"
                        "  <body>\n"
                        "    <h1>" BANNER "</h1>\n"
                        "    <p>Click the button to toggle the state of the outlet.</p>\n"
                        "    <form>\n"
                        "      <input type=\"submit\" value=\"Toggle Outlet\" formaction=\"/?" TOGGLE_QUERY "\""
                        "        formmethod=\"post\"/>\n"
                        "    </form>\n"
                        "  </body>\n"
                        "</html>\n\r\n";

/**
 * @brief Invert the state of the outlet. I.e., if it was on, turn it (and the LED) off and vice versa.
 * 
 */
void toggleOutlet() {
    static uint8_t relayState = RELAY_OPEN;
    relayState = relayState == RELAY_CLOSED ? RELAY_OPEN : RELAY_CLOSED;
    digitalWrite(RELAY, relayState);
    digitalWrite(LED, relayState == RELAY_CLOSED ? LED_LIT : LED_DARK);
}

/**
 * @brief   HTTP GET and HEAD method handler for webServer. Return the complete message for the 
 *          request -- headers and, if applicable, content -- that is the response to a GET (or 
 *          HEAD) request for the specified trPath and trQuery.
 * 
 * @param webServer         The SimpleWebServer for which we're acting as the GET and HEAD handlers
 * @param trPath            The path portion of the URI of the resource being requested
 * @param trQuery           The trQuery (if any)
 * @return String           The message to be sent to the client
 */
String handleGetAndHead(SimpleWebServer* webServer, String trPath, String trQuery) {
    // Assume the response message starts with the usual response headers
    String message = swsNormalResponseHeaders;

    // We only have a "home page." If that's what was asked for, proceed
    if (trPath.equals("/") || trPath.equals("/index.html") || trPath.equals("index.htm")) {
        // If this is a GET request, add the content; for HEAD requests the headers are all there is
        if (webServer->httpMethod() == swsGET) {
            message += homePage;
        }

    // Otherwise, they asked for some other resource. We don't have it. Respond with "404 Not Found" message
    } else {
        message = swsNotFoundResponseHeaders;
    }
    return message;
}

/**
 * @brief   HTTP POST method handler for webServer. Return the complete message for the 
 *          request -- headers and, if applicable,content -- that is the response to a 
 *          POST for the specified trPath and trQuery.
 * 
 * @param webServer         The SimpleWebServer for which we're acting as the POST handler
 * @param trPath            The path portion of the URI of the resource being requested
 * @param trQuery           The trQuery (if any)
 * @return String           The message to be sent to the client
  */
String handlePost(SimpleWebServer* webServer, String trPath, String trQuery) {
    // Assume the response starts with the usual response headers
    String message = swsNormalResponseHeaders;
    // We only respond well to a POST to the "home page" with a TOGGLE_QUERY query
    if ((trPath.equals("/") || trPath.equals("/index.html") || trPath.equals("index.htm")) && 
        trQuery.equalsIgnoreCase(TOGGLE_QUERY)) {
        message += homePage;
        toggleOutlet();
    // Otherwise, they requested something we can't deal with. Respond with "400 Bad Request" message
    } else {
        message = swsBadRequestResponseHeaders;
    }
    return message;
}

/**
 * @brief The "unrecognized command" command handler. Called by the ui object as needed.
 * 
 */
void onUnrecognized() {
    Serial.printf("Command \"%s\" is not recognized.\n", ui.getWord(0).c_str());
}

/**
 * @brief The "help" and "h" ui command handler. Called by ui object as needed.
 * 
 */
void onHelp() {
    Serial.print(
        "Help for " BANNER "\n"
        "  help             Print this text\n"
        "  h                Same as 'help'\n"
        "  ssid [<ssid>]    Print or set the ssid of the WiFi AP we should connect to\n"
        "  pw [<password>]  Print or set the password we are to use to connect\n"
        "  save             Save the current ssid and password and continue\n"
        "  status           Print the status of the system\n"
    );
}

/**
 * @brief The ssid ui command handler. Called by the ui object as needed.
 * 
 */
void onSsid() {
    String ssid = ui.getCommandLine().substring(ui.getWord(0).length());
    ssid.trim();
    unsigned int ssidLen = ssid.length();
    if (ssidLen != 0 && ssidLen < sizeof(config.ssid)) {
        strcpy(config.ssid, ssid.c_str());
        Serial.printf("Set SSID to \"%s\"\n", config.ssid);
    } else if (ssidLen == 0) {
        Serial.printf("SSID is \"%s\"\n", config.ssid);
    } else {
        Serial.printf("Specified SSID is too long. Maximum length is %d\n", sizeof(config.ssid) - 1);
    }
}

/**
 * @brief The pw ui command handler. Called by the ui object as needed.
 * 
 */
void onPw() {
    String pw = ui.getCommandLine().substring(ui.getWord(0).length());
    pw.trim();
    unsigned int pwLen = pw.length();
    if (pwLen > 0 && pwLen < sizeof(config.password)) {
        strcpy(config.password, pw.c_str());
        Serial.printf("Set password to \"%s\"\n", config.password);
    } else if (pwLen == 0) {
        Serial.printf("Password is \"%s\"\n", config.password);
    } else {
        Serial.printf("Password is too long. Maximum length is %d", sizeof(config.password) - 1);
    }
}

/**
 * @brief The save ui command handler. Called by the ui object as needed.
 * 
 */
void onSave() {
    config.signature = CONFIG_SIG;
    EEPROM.put(0, config);
    if (EEPROM.commit()) {
        Serial.print("WiFi configuration saved. Restarting to connect using the new credentials.\n");
        ESP.restart();
    }
    Serial.print("Credential saving failed. No diagnostic information available.\n");
}

/**
 * @brief The status ui command handler. Called by the ui object as needed.
 * 
 */
void onStatus(){
    if (running) {
        Serial.printf(
            "We're attached to WiFi SSID \"%s\".\n"
            "There our local IP address is ", 
            config.ssid);
        Serial.println(WiFi.localIP());
    }
    Serial.printf(
        "The web server is %srunning.\n"
        "The outlet is %s\n",
        running ? "" : "not ", digitalRead(RELAY) == RELAY_CLOSED ? "on" : "off");
}

/**
 * @brief The Arduino setup function. Called once at power-on or reset
 * 
 */
void setup() {
    // Do the basic hardware initialization
    Serial.begin(9600);                 // Get Serial via UART up and running
    delay(SERIAL_CONN_MILLIS);
    pinMode(LED, OUTPUT);               // Initialize the LED
    digitalWrite(LED, LED_DARK);
    pinMode(RELAY, OUTPUT);             // Initialize the relay
    digitalWrite(RELAY, RELAY_OPEN);
    button.begin();                     // Initialize the button

    // Attach the ui command handlers
    ui.attachDefaultCmdHandler(onUnrecognized);
    if (!(
        ui.attachCmdHandler("help", onHelp) &&
        ui.attachCmdHandler("h", onHelp) &&
        ui.attachCmdHandler("ssid", onSsid) &&
        ui.attachCmdHandler("pw", onPw) &&
        ui.attachCmdHandler("save", onSave) &&
        ui.attachCmdHandler("status", onStatus))
        ) {
        Serial.print("Couldn't attach all the ui command handlers.\n");
    }

    Serial.println(BANNER);             // Say hello

    // See if we have our configuration data available
    EEPROM.begin(sizeof(eepromData_t));
    // If so try to get it from EEPROM
    if (EEPROM.percentUsed() != -1) {
        EEPROM.get(0,config);
    }
    // If the configuration signature is not what we expect, we don't have credentials
    running = config.signature == CONFIG_SIG;
    if (running) {
        // Get the WiFi connection going
        Serial.printf("\nConnecting to %s ", config.ssid);
        unsigned long startMillis = millis();
        WiFi.begin(config.ssid, config.password);
        while (WiFi.status() != WL_CONNECTED && millis() - startMillis < WIFI_CONN_MILLIS) {
            delay(WIFI_DELAY_MILLIS);
            Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print(" WiFi connected.\nIP address is ");
            Serial.println(WiFi.localIP());
            wiFiServer.begin();
            // Get the webServer going and attach the HTTP method handlers
            webServer.begin(wiFiServer);
            webServer.attachMethodHandler(swsGET, handleGetAndHead);
            webServer.attachMethodHandler(swsHEAD, handleGetAndHead);
            webServer.attachMethodHandler(swsPOST, handlePost);
        } else {
            Serial.printf("Unable to connect to WiFi. Status: %d\n", WiFi.status());
            running = false;
        }
    } else {
        Serial.print("No stored WiFi credentials found.\n");
    }
    if (!running) {
        Serial.print(
            "Unable to get the web server up and running.\n"
            "Use command line to set the WiFi credentials if needed./n"
            "Type \"help\" for help.\n");
    }
}

/**
 * @brief The Arduino loop function. Called repeatedly
 * 
 */
void loop() {

    // Let the ui do its thing
    ui.run();

    // Deal with button clicks
    if (button.clicked()) {
            toggleOutlet();
    }
    
    // If the webServer is running, let it do its thing.
    if (running) {
        // Let the web server do its thing
        webServer.run();
    }
}