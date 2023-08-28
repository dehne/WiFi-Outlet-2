/****
 * @file main.cpp
 * @version 1.0.0
 * @date August 28, 2023
 * 
 * WiFi Outlet -- Replacement firmware for the 2017 Sharper Image model 70011 WiFi 
 * controlled outlet. 
 * 
 * About this project
 * ==================
 * 
 * This sketch presents the Sharper Image model 70011 WiFi controlled outlet as a one-page web 
 * site on the local WiFi network. The web page shows two types of schedule for turning the 
 * outlet on and off at specified times of day. One schedule provides up to three on-off cycles 
 * per day. The other has one on-off cycle for weekdays and another for weekend days. Using the 
 * page you can set the times and which schedule to use. The page also lets you turn the outlet 
 * on and off manually.
 * 
 * There's a button on the device. Clicking it toggles the outlet on or off.
 * 
 * The implementation uses -- in addition to all the ESP8266 WiFi stuff -- a super simple web 
 * server I wrote for the purpose. See SimpleWebServer.h for details. It also uses two other 
 * libraries I wrote for other projects, UserInput, which makes having a commandline 
 * interpreter easy to do, and PushButton, for simple clicky-button support. See them for more 
 * information.
 * 
 * Notes on the hardware
 * =====================
 * 
 * - The TYWE3S daughterboard in this device contains an ESP8266 microprocessor, SPI flash and 
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
 * - GPIO0 is connected to one side of the button on the device. The other side is connected to 
 *   Gnd. So, "active LOW."
 * 
 * - GPIO13 is connected to one side of the LED. The other side is connected, via a resistor, to 
 *   Vcc. So, the LED is "active LOW."
 * 
 * - GPIO14 is connected to the gate of transistor Q1, the driver for the relay that turns the 
 *   outlet on and off. It's "active HIGH." To turn the outlet on, hold GPIO14 HIGH.
 * 
 * - The other GPIOs and ADC aren't hooked to anything, so far as I know.
 * 
 * - To hack the device, you'll need to solder wires to Gnd, Vcc, RXD0, TXD0 and, for convenience, 
 *   to GPIO0 and RST. Connect all but the last two of these to an FTDI serial-to-USB device via 
 *   female Dupont connectors (Gnd --> GND, Vcc --> 3V3, RXD0 --> TXD, and TXD0 --> RXD) and a 
 *   5-pin connector shell. Put a female connector and shell on the GPIO0 wire and a male one on 
 *   the wire from RST. Find a place on the board connected to Gnd and solder a piece of wire to 
 *   act as a header pin there. (The pads for the unoccupied R24 and R28 nearest the electrolytic 
 *   capacitor worked for me.) Hot-glue the wires to the side of the relay for strain relief.
 * 
 * - If you keep the wire lengths for the above to about 10cm you can coil them up inside the 
 *   device when you reassemble it. (When reassembling, cover up the exposed male connector on the 
 *   RST wire, or remove it and the GPIO0 wire; you won't need them once the device is put back 
 *   together.)
 * 
 * - There's even room on the case between the outlet and the button for a rectangular hole to 
 *   mount and expose the 5-pin Dupont connector. That will let you reprogram the thing with it 
 *   all put back together.
 * 
 * - To put the ESP8266 into "PGM from UART" mode, making it ready to accept a firmware update, 
 *   GPIO00 needs to be connected to Gnd when the ESP8266 is reset or powered up. That can be 
 *   done by attaching the wire from GPIO0 to the new header pin. Leaving GPIO0 floating at 
 *   power-on or reset results in the ESP8266 entering "Boot from SPI Flash mode." I.e., running 
 *   normally. 
 * 
 * - When the ESP8266 is "soft reset" in "PGM from UART" mode, which is what happens after 
 *   Platformio loads new firmware into it, the processor will go into "Boot from SPI Flash" mode, 
 *   even with GPIO0 attached to Gnd. 
 * 
 * - When the ESP8266 is reset using the ESP.reset() function, it DOES pay attention to GPIO0 and 
 *   will enter "PGM from UART" mode if GPIO0 is attached to Gnd. 
 * 
 * - To hardware reset the ESP8266, momentarily connect the wire from RST to Gnd or hold down the 
 *   push-button on the case while you connect to the FTDI device. 
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
#include <TZ.h>                                     // POSIX timezone strings (for reference)
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
#define BANNER              "WiFi Switch V1.0.0"    // The tagline to identify this sketch
#define NTP_SERVER          "pool.ntp.org"          // The NTP server we use
#define TOGGLE_QUERY        "outlet=toggle"         // The URI query string to cause the outlet to toggle state
#define SCHED_UPDATE_QUERY  "schedule=update"       // The URI query string to cause the schedule parms to be updated
#define SCHED_TOGGLE_QUERY  "schedule=toggle"       // The URI query string to cause the schedule enable/disable toggle
#define DAILY_SCHED_VALUE   "daily"                 // The value of the <input type="radio" /> selecting the daily schedule
#define WEEKLY_SCHED_VALUE  "weekly"                // The value of the <input type="radio" /> selecting the weekly schedule

#define LED_LIT             (LOW)                   // digitalWrite value to light the LED
#define LED_DARK            (HIGH)                  // digitalWrite value to turn the LED off
#define RELAY_OPEN          (LOW)                   // digitlWrite value to open the relay
#define RELAY_CLOSED        (HIGH)                  // digitalWrite value to close the relay
#define OUTLET_ON           (true)                  // For setOutlet()
#define OUTLET_OFF          (false)                 // For setOutlet()
#define SERIAL_CONN_MILLIS  (4000)                  // millis() to wait after Serial.begin() to start using it
#define WIFI_DELAY_MILLIS   (500)                   // millis() to delay between printing "." while waitinf for WiFi
#define WIFI_CONN_MILLIS    (15000)                 // millis() to wait for WiFi connect before giving up
#define NTP_SET_MILLIS      (10000)                 // millis() to wait for NTP server to set the time
#define DAWN_OF_HISTORY     (1533081600)            // Well, actually August 1st, 2018
#define CONFIG_SIG          (0x3940)                // Our "signature" in EEPROM to know the data is (probably) ours

typedef unsigned int minPastMidnight_t;             // Minutes past midnight: 0 --> 1399

struct eepromData_t {
    uint16_t signature;                             // Random integer identifying the data as ours. Change when shape changes
    char ssid[33];                                  // SSID of the WiFi network we should use.
    char password[64];                              // Password to use to connect to the WiFi
    char timeZone[32];                              // The POSIX timezone string for the timezone we use (see TZ.h)
    bool enabled;                                   // If true, the schedule is enabled; false schedule is diabled
    bool useWeekly;                                 // If true we should use the weekly schedule, false, use the daily
    bool dailyEnable[3];                            // Whether each of the three daily on/off periods is enabled
    minPastMidnight_t dailyOnTime[3];               // The turn-on time for each of the three daily on/off periods
    minPastMidnight_t dailyOffTime[3];              // The turn-off time for each of the three daily on/off periods
    minPastMidnight_t weekdayOnTime;                // The turn-on time for weekdays
    minPastMidnight_t weekdayOffTime;               // The turn-off time for weekdays
    minPastMidnight_t weekendOnTime;                // The turn-on time for weekends
    minPastMidnight_t weekendOffTime;               // The turn-off time for weekends
};

WiFiServer wiFiServer {80};
SimpleWebServer webServer;
PushButton button {BUTTON};
UserInput ui {};
//                   sig ssid pw  timezone                  enabled useWeekly --- dailyEnable --
eepromData_t config {0,  "",  "", "PST8PDT,M3.2.0,M11.1.0", false,  false,    true, false, false, 
//                   -- dailyOnTime ---  -- dailyOffTime ----
                     8*60, 13*60, 19*60, 12*60, 17*60 , 21*60, 
//                   wkDyOn wkDyOff wkEnOn wkEnOff
                     7*60,  17*60,  18*60, 22*60};  // Default config

enum formDataName_t : uint8_t {fdStype, fdDg0, fdDg1, fdDg2, fdDg0n, fdDg0f, fdDg1n, fdDg1f, fdDg2n, fdDg2f, fdWdn, fdWdf, fdWen, fdWef, _formDataNameSize_};
String formDataNames[_formDataNameSize_] =
                              {"stype", "Dg0", "Dg1", "Dg2", "Dg0n", "Dg0f", "Dg1n", "Dg1f", "Dg2n", "Dg2f", "wdn", "wdf", "wen", "wef"};
bool running;                                       // False when can't get WiFi, time, etc.
bool scheduleUpdated;                               // True when schedule updated since last looked at by followSchedule()

/**
 * @brief   Convert a string in the form "hh:mm", hh = 00..23, mm = 00..59 to minPastMidnight_t
 *          There's no malformed input checking here!
 * 
 * @param hhcmm                 The string "hh:mm" that's to be converted
 * @return minPastMidnight_t    The equivalent in minutes past midnight
 */
minPastMidnight_t toMinsPastMidnight(String hhcmm) {
    return (hhcmm.substring(0, 2).toInt() * 60) + hhcmm.substring(3, 5).toInt();
}

/**
 * @brief   Convert from minPastMidnight_t to "hh:mm".
 * 
 * @param minsPastMidnight 
 * @return String 
 */
String fromMinsPastMidnight(minPastMidnight_t minsPastMidnight) {
    int mm = minsPastMidnight % 60;
    int hh = minsPastMidnight / 60;
    return (hh < 10 ? String(0) + String(hh) : String(hh)) + ":" + (mm < 10 ? String(0) + String(mm) : String(mm));
}

/**
 * 
 * @brief Set the system clock to the current local time and date using an NTP server. 
 * 
 * @return true   Time set successfully
 * @return false  Unable to set the time. Don't trust the system time
 * 
 */
bool setClock() {
    configTzTime(config.timeZone, NTP_SERVER);
    time_t nowSecs = 0;
    unsigned long startMillis = millis();
    do {
        Serial.print(F("Waiting for NTP time sync..."));
        nowSecs = time(nullptr);
    } while (nowSecs < DAWN_OF_HISTORY && millis() - startMillis < NTP_SET_MILLIS);
    if (nowSecs > DAWN_OF_HISTORY) {
        Serial.printf("Sync successful. Current time: %s", ctime(&nowSecs)); // ctime() appends a "\n", just because.
        return true;
    } else {
        Serial.print("Unable to set the time.\n");
        return false;
    }
}

/**
 * @brief Utility function to save the config data and optionally print a message upon success
 * 
 * @param successMessage    The message to print to Serial if save succeeds. omitted ==> no message
 * @return true             Save succeeded
 * @return false            Save failed
 */
bool saveConfig(String successMessage = "") {
    EEPROM.put(0, config);
    if (EEPROM.commit()) {
        if (successMessage.length() != 0) {
            Serial.print(successMessage);
        }
        return true;
    }
    Serial.print("[saveConfig] Configuration update failed. No diagnostic information available.\n");
    return false;
}

/**
 * @brief Return the state of the outlet.
 * 
 * @return true     The outlet is turned on
 * @return false    The outlet is turned off
 */
bool outletIsOn() {
    return digitalRead(RELAY) == RELAY_CLOSED;
}

/**
 * @brief Invert the state of the outlet. I.e., if it was on, turn it (and the LED) off and vice versa.
 * 
 */
void toggleOutlet() {
    uint8_t newRelayState = digitalRead(RELAY) == RELAY_CLOSED ? RELAY_OPEN : RELAY_CLOSED;
    digitalWrite(RELAY, newRelayState);
    digitalWrite(LED, newRelayState == RELAY_CLOSED ? LED_LIT : LED_DARK);
}

/**
 * @brief   Turn the outlet on or off as specified by outletOn value
 * 
 * @param outletOn  true ==> outlet turns on, false ==> outlet turns off
 */
void setOutletTo(bool outletOn) {
    digitalWrite(RELAY, outletOn ? RELAY_CLOSED : RELAY_OPEN);
    digitalWrite(LED, outletOn ? LED_LIT : LED_DARK);

    #ifdef DEBUG
    Serial.printf("  Turned outlet %s.\n", outletOn ? "on" : "off");
    #endif
}

/**
 * @brief Assemble the current state of our home page and send it to the httpClient.
 * 
 * @param httpClient    The HTTP client we are to send the assembled home page to.
 */
void sendHomePage(WiFiClient* httpClient) {
// The HTML for the device's "home page"
    String pageHtml = "<!doctype html>\n"
                        "<html>\n"
                        "<head>\n"
                        "<meta charset=\"utf-8\">\n"
                        "<title>WiFi Outlet</title>\n"
                        "<style>\n"
                        "body {\n"
                        "background-color: black;\n"
                        "color: antiquewhite;\n"
                        "font-family: \"Gill Sans\", \"Gill Sans MT\","
                        " \"Myriad Pro\", \"DejaVu Sans Condensed\", Helvetica, Arial, \"sans-serif\";\n"
                        "}\n"
                        "h1 {\n"
                        "text-align: center;\n"
                        "font-family: Cambria, \"Hoefler Text\", \"Liberation Serif\", Times,"
                        " \"Times New Roman\", \"serif\";\n"
                        "}\n"
                        "td {\n"
                        "text-align: center;\n"
                        "}\n"
                        "</style>\n"
                        "</head>\n"
                        "<body>\n"
                        "<h1>WiFi Outlet Control Panel</h1>\n"
                        "<p>Using this page you can set up and control your WiFi outlet. You can set up daily and weekly "
                        "schedules, and switch between which to follow. You can also turn schedule-following on and off "
                        "or just manually control the outlet.</p>\n"
                        "<form method=\"post\">\n"
                        "<h2>Schedule</h2>\n"
                        "<table width=\"80%\" border=\"0\" cellpadding=\"10\">\n"
                        "<tbody>\n"
                        "<tr>\n"
                        "<td>\n"
                        "<input type=\"radio\" id=\"daily\" name=\"stype\" value=\"" DAILY_SCHED_VALUE "\" @dailyChecked />\n"
                        "<label for=\"daily\">daily</label>\n"
                        "</td>\n"
                        "<td>\n"
                        "<table width=\"100%\" border=\"0\">\n"
                        "<tbody>\n"
                        "<tr bgcolor=\"#3A3A3A\">\n"
                        "<td colspan=\"2\">\n"
                        "<input type=\"checkbox\" name=\"Dg0\" @dg0Checked />\n"
                        "<label for=\"Dg0\">Enable</label>\n"
                        "</td>\n"
                        "<td colspan=\"2\">\n"
                        "<input type=\"checkbox\" name=\"Dg1\" @dg1Checked />\n"
                        "<label for=\"Dg1\">Enable</label>\n"
                        "</td>\n"
                        "<td colspan=\"2\">\n"
                        "<input type=\"checkbox\" name=\"Dg2\" @dg2Checked />\n"
                        "<label for=\"Dg2\">Enable</label>\n"
                        "</td>\n"
                        "</tr>\n"
                        "<tr bgcolor=\"#3A3A3A\">\n"
                        "<td>Turn On at</td>\n"
                        "<td>Turn Off at</td>\n"
                        "<td>Turn On at</td>\n"
                        "<td>Turn Off at</td>\n"
                        "<td>Turn On at</td>\n"
                        "<td>Turn Off at</td>\n"
                        "</tr>\n"
                        "<tr>\n"
                        "<td><input type=\"time\" name=\"Dg0n\" value=\"@dg0n\" ></td>\n"
                        "<td><input type=\"time\" name=\"Dg0f\" value=\"@dg0f\"></td>\n"
                        "<td><input type=\"time\" name=\"Dg1n\" value=\"@dg1n\"></td>\n"
                        "<td><input type=\"time\" name=\"Dg1f\" value=\"@dg1f\"></td>\n"
                        "<td><input type=\"time\" name=\"Dg2n\" value=\"@dg2n\"></td>\n"
                        "<td><input type=\"time\" name=\"Dg2f\" value=\"@dg2f\"></td>\n"
                        "</tr>\n"
                        "</tbody>\n"
                        "</table>\n"
                        "</td>\n"
                        "</tr>\n"
                        "<tr>\n"
                        "<td>\n"
                        "<input type=\"radio\" id=\"weekly\" name=\"stype\" value=\"" WEEKLY_SCHED_VALUE "\" @weeklyChecked />\n"
                        "<label for=\"weekly\">weelky</label>\n"
                        "</td>\n"
                        "<td>\n"
                        "<table width=\"100%\" border=\"0\">\n"
                        "<tbody>\n"
                        "<tr bgcolor=\"#3A3A3A\">\n"
                        "<td colspan=\"2\">Weekday</td>\n"
                        "<td colspan=\"2\">Weekend</td>\n"
                        "</tr>\n"
                        "<tr bgcolor=\"#3A3A3A\">\n"
                        "<td>Turn On at</td>\n"
                        "<td>Turn Off at</td>\n"
                        "<td>Turn On at</td>\n"
                        "<td>Turn Off at</td>\n"
                        "</tr>\n"
                        "<tr>\n"
                        "<td><input type=\"time\" name=\"wdn\" value=\"@wdn\"></td>\n"
                        "<td><input type=\"time\" name=\"wdf\" value=\"@wdf\"></td>\n"
                        "<td><input type=\"time\" name=\"wen\" value=\"@wen\"></td>\n"
                        "<td><input type=\"time\" name=\"wef\" value=\"@wef\"></td>\n"
                        "</tr>\n"
                        "</tbody>\n"
                        "</table>\n"
                        "</td>\n"
                        "</tr>\n"
                        "</tbody>\n"
                        "</table>\n"
                        "<p><input type=\"submit\" value=\"Save schedule\" formaction=\"/index.html?" SCHED_UPDATE_QUERY "\" /></p>\n"
                        "<p>The shedule is currently @schedIs. To @schedWillBe it, click the button below.</p>\n"
                        "<p><input type=\"submit\" value=\"@schEnButton schedule\" formaction=\"/index.html?" SCHED_TOGGLE_QUERY "\" /></p>\n"
                        "<h2>Manual Control</h2>\n"
                        "<p>The outlet is currently @outletIs. To turn it @outletWillBe click the button below.</p>\n"
                        "<input type=\"submit\" value=\"Turn outlet @outletWillBe\" formaction=\"/index.html?" TOGGLE_QUERY "\" />\n"
                        "</form>\n"
                        "</body>\n"
                        "</html>\r\n"
                        "\r\n";

    // Substitute all the variable information needed in the HTLM. Each variable in the text is a name beginning with "@"
    pageHtml.replace("@dailyChecked", config.useWeekly ? "" : "checked");
    pageHtml.replace("@dg0Checked", config.dailyEnable[0] ? "checked" : "");
    pageHtml.replace("@dg0n", fromMinsPastMidnight(config.dailyOnTime[0]));
    pageHtml.replace("@dg0f", fromMinsPastMidnight(config.dailyOffTime[0]));
    pageHtml.replace("@dg1Checked", config.dailyEnable[1] ? "checked" : "");
    pageHtml.replace("@dg1n", fromMinsPastMidnight(config.dailyOnTime[1]));
    pageHtml.replace("@dg1f", fromMinsPastMidnight(config.dailyOffTime[1]));
    pageHtml.replace("@dg2Checked", config.dailyEnable[2] ? "checked" : "");
    pageHtml.replace("@dg2n", fromMinsPastMidnight(config.dailyOnTime[2]));
    pageHtml.replace("@dg2f", fromMinsPastMidnight(config.dailyOffTime[2]));
    pageHtml.replace("@weeklyChecked", config.useWeekly ? "checked" : "");
    pageHtml.replace("@wdn", fromMinsPastMidnight(config.weekdayOnTime));
    pageHtml.replace("@wdf", fromMinsPastMidnight(config.weekdayOffTime));
    pageHtml.replace("@wen", fromMinsPastMidnight(config.weekendOnTime));
    pageHtml.replace("@wef", fromMinsPastMidnight(config.weekendOffTime));
    pageHtml.replace("@schedIs", config.enabled ? "enabled" : "disabled");
    pageHtml.replace("@schedWillBe", config.enabled ? "disable" : "enable");
    pageHtml.replace("@schEnButton", config.enabled ? "Disable" : "Enable");
    pageHtml.replace("@outletIs", outletIsOn() ? "on" : "off");
    pageHtml.replace("@outletWillBe", outletIsOn() ? "off" : "on");

    httpClient->print(pageHtml);        // Send the complete text to the client.
}

/**
 * @brief   HTTP GET and HEAD method handler for webServer. Send the complete message for the 
 *          request -- headers and, if applicable, content -- that is the response to a GET (or 
 *          HEAD) request for the specified trPath and trQuery to the httpClient.
 * 
 * @param webServer         The SimpleWebServer for which we're acting as the GET and HEAD handlers.
 * @param httpClient        The HTTP Client making the request.
 * @param trPath            The path portion of the URI of the resource being requested.
 * @param trQuery           The trQuery (if any).
 */
void handleGetAndHead(SimpleWebServer* webServer, WiFiClient* httpClient, String trPath, String trQuery) {

    // We only have a "home page." If that's what was asked for, proceed.
    if (trPath.equals("/") || trPath.equals("/index.html") || trPath.equals("index.htm")) {
        httpClient->print(swsNormalResponseHeaders);
        // If this is a GET request, add the content; for HEAD requests the headers are all there is.
        if (webServer->httpMethod() == swsGET) {
            sendHomePage(httpClient);
        #ifdef DEBUG
            Serial.print("GET request for home page received and processed.\n");
            ui.cancelCmd();
        } else {
            Serial.print("HEAD request received and processed.\n");
            ui.cancelCmd();
        #endif
        }

    // Otherwise, they asked for some other resource. We don't have it. Respond with "404 Not Found" message.
    } else {
        httpClient->print(swsNotFoundResponseHeaders);
        Serial.print("GET or HEAD request received for something other than the home page. Sent \"404 not found\"\n");
        ui.cancelCmd();
    }
}

/**
 * @brief   HTTP POST method handler for webServer. Send the complete message for the 
 *          request -- headers and, if applicable, content -- that is the response to a 
 *          POST for the specified trPath and trQuery to the httpClient.
 * 
 * @param webServer         The SimpleWebServer for which we're acting as the POST handler.
 * @param httpClient        The HTTP client making the request.
 * @param trPath            The path portion of the URI of the resource being requested.
 * @param trQuery           The trQuery portion of the URI (if any).
 */
void handlePost(SimpleWebServer* webServer, WiFiClient* httpClient, String trPath, String trQuery) {
    // We only respond well to a POST to the "home page".
    if (trPath.equals("/") || trPath.equals("/index.html") || trPath.equals("index.htm")) {

        // Deal with TOGGLE_QUERY -- flip the state of the outlet on --> off or vice versa
        if (trQuery.equalsIgnoreCase(TOGGLE_QUERY)) {
            toggleOutlet();
            #ifdef DEBUG
            Serial.printf("[handlePost] Outlet has been turned %s.\n", outletIsOn() ? "on" : "off");
            #endif

        // Deal with SCHED_TOGGLE_QUERY -- flip the state of the schedule enabled --> disabled or vice versa
        } else if (trQuery.equalsIgnoreCase(SCHED_TOGGLE_QUERY)) {
            config.enabled = !config.enabled;
            saveConfig();
            #ifdef DEBUG
            Serial.printf("[handlePost] Schedule has been %s.\n", config.enabled ? "enabled" : "disabled");
            #endif

        // Deal with SCHED_UPDATE_QUERY -- store the state of the schedule in EEPROM
        } else if (trQuery.equalsIgnoreCase(SCHED_UPDATE_QUERY)) {
            #ifdef DEBUG
            Serial.printf("[handlePost] Update schedule. Message headers: \"%s\"\n", webServer->clientHeaders().c_str());
            Serial.printf("[handlePost] The message body is \"%s\"\n", webServer->clientBody().c_str());
            ui.cancelCmd(); // Reissue command prompt after the print
            #endif
            config.dailyEnable[0] = config.dailyEnable[1] = config.dailyEnable[2] = false; // N.B. Only sent in POST data when true
            for (uint8_t i = 0; i < _formDataNameSize_; i++) {
                String formValue = webServer->getFormDatum(formDataNames[i]);
                if (formValue.length() != 0) {
                    switch ((formDataName_t)i) {
                        case fdStype:
                            config.useWeekly = formValue.equals(WEEKLY_SCHED_VALUE);
                            break;
                        case fdDg0:
                            config.dailyEnable[0] = formValue == "on" ? true : false;
                            break;
                        case fdDg1:
                            config.dailyEnable[1] = formValue == "on" ? true : false;
                            break;
                        case fdDg2:
                            config.dailyEnable[2] = formValue == "on" ? true : false;
                            break;
                        case fdDg0n:
                            config.dailyOnTime[0] = toMinsPastMidnight(formValue);
                            break;
                        case fdDg0f:
                            config.dailyOffTime[0] = toMinsPastMidnight(formValue);
                            break;
                        case fdDg1n:
                            config.dailyOnTime[1] = toMinsPastMidnight(formValue);
                            break;
                        case fdDg1f:
                            config.dailyOffTime[1] = toMinsPastMidnight(formValue);
                            break;
                        case fdDg2n:
                            config.dailyOnTime[2] = toMinsPastMidnight(formValue);
                            break;
                        case fdDg2f:
                            config.dailyOffTime[2] = toMinsPastMidnight(formValue);
                            break;
                        case fdWdn:
                            config.weekdayOnTime = toMinsPastMidnight(formValue);
                            break;
                        case fdWdf:
                            config.weekdayOffTime = toMinsPastMidnight(formValue);
                            break;
                        case fdWen:
                            config.weekendOnTime = toMinsPastMidnight(formValue);
                            break;
                        case fdWef:
                            config.weekendOffTime = toMinsPastMidnight(formValue);
                            break;
                        default:
                            break;
                    }
                }
            }
            // Save the new data in config to EEPROM
            #ifdef DEBUG
            saveConfig("[handlePost] Configuration update saved.\n");
            #else
            saveConfig();
            #endif
            scheduleUpdated = true;     // Let followSchedule() know we've updated the schedule 
            
        // Deal with a query we don't understand
        } else {
            Serial.printf("POST request received for query we don't understand: \"%s\"./n", trQuery.c_str());
            ui.cancelCmd(); // Reissue command prompt after print
            httpClient->print(swsBadRequestResponseHeaders);
            return;
        }
        httpClient->print("HTTP/1.1 303 See other\r\n"
                        "Location: /index.html\r\n\r\n");
        return;
    }
    // The client requested something we can't deal with. Respond with "400 Bad Request" message.
    Serial.printf("POST request recived for something other than toggle outlet. path: \"%s\" query: \"%s\"./n",
        trPath.c_str(), trQuery.c_str());
    ui.cancelCmd(); // Reissue command prompt after print
    httpClient->print(swsBadRequestResponseHeaders);
}

/**
 * @brief Utility function to follow the schedule defined by config. 
 * 
 */
void followSchedule() {
    static minPastMidnight_t lastMinPastMidnight = 0;       // When we last noticed the time change
    time_t curTime = time(nullptr);
    struct tm *timeval;
    timeval = localtime(&curTime);
    minPastMidnight_t curMinPastMidnight = timeval->tm_hour * 60 + timeval->tm_min;

    // If the schedule is turned off, or the time hasn't changed and there's no new schedule, nothing to do
    if (!config.enabled || (curMinPastMidnight == lastMinPastMidnight && !scheduleUpdated)) {
        return;
    }

    #ifdef DEBUG
    if (scheduleUpdated) {
        Serial.print("[followSchedule] Got a new schedule to follow.\n");
        scheduleUpdated = false;    // Remember we saw the new schedule
    }

    Serial.printf("[followSchedule] Checking the schedule at %s (%d).\n", 
        fromMinsPastMidnight(curMinPastMidnight).c_str(), curMinPastMidnight);
    #else
    scheduleUpdated = false;
    #endif

    if (config.useWeekly) {
        // Deal with weekly schedule. First, is it a weekday or weekend
        bool isWeekday = (timeval->tm_wday >= 1 && timeval->tm_wday <= 5);

        #ifdef DEBUG
        Serial.printf("[followSchedule] A weekly schedule on a %s.\n", isWeekday ? "weekday" : "weekend day");
        #endif

        if (isWeekday) {
            // Deal with weekday in a weekly schedule

            #ifdef DEBUG
            Serial.printf("  Looking for on time of %s and an off time of %s.\n", 
                fromMinsPastMidnight(config.weekdayOnTime).c_str(), fromMinsPastMidnight(config.weekdayOffTime).c_str());
            #endif

            if (config.weekdayOffTime == curMinPastMidnight) {
                setOutletTo(OUTLET_OFF);
            }
            if (config.weekdayOnTime == curMinPastMidnight) {
                setOutletTo(OUTLET_ON);
            }
        } else {
            // Deal with weekend day in a weekly schedule

            #ifdef DEBUG
            Serial.printf("  Looking for on time of %s and an off time of %s.\n", 
                fromMinsPastMidnight(config.weekendOnTime).c_str(), fromMinsPastMidnight(config.weekendOffTime).c_str());
            #endif

            if (config.weekendOffTime == curMinPastMidnight) {
                setOutletTo(OUTLET_OFF);
            }
            if (config.weekendOnTime == curMinPastMidnight) {
                setOutletTo(OUTLET_ON);
            }
        }
    } else {
        #ifdef DEBUG
        Serial.print("[followSchedule] Following a daily schedule.\n");
        #endif
        // Deal with daily schedule

        for (uint8_t group = 0; group < sizeof(config.dailyEnable); group++) {
            if (config.dailyEnable[group]) {

                #ifdef DEBUG
                Serial.printf("  Group %d is enabled. Checking for an on time of %s and an off time of %s.\n",
                    group, fromMinsPastMidnight(config.dailyOnTime[group]).c_str(), fromMinsPastMidnight(config.dailyOffTime[group]).c_str());
                #endif

                if (config.dailyOffTime[group] == curMinPastMidnight) {
                    setOutletTo(OUTLET_OFF);
                }
                if (config.dailyOnTime[group] == curMinPastMidnight) {
                    setOutletTo(OUTLET_ON);
                }
            #ifdef DEBUG
            } else {
                Serial.printf("  Group %d is disabled.\n", group);
            #endif
            }
        }
    }
    lastMinPastMidnight = curMinPastMidnight;               // We handled things for the current time
    #ifdef DEBUG
    ui.cancelCmd();
    #endif
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
        "  tz [<timezone>]  Print or set the POSIX time zone string for the time zone we are in\n"
        "  save             Save the current ssid and password and continue\n"
        "  status           Print the status of the system\n"
        "  restart          Restart the device. E.g., to use newly saved WiFi credentials.\n"
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
 * @brief The tz ui command handler. Called by the ui object as needed.
 * 
 */
void onTz() {
    String tz = ui.getWord(1);
    if (tz.length() == 0) {
        Serial.printf("Timezone is set to %s\n", config.timeZone);
    } else if (tz.length() < sizeof(config.timeZone)) {
        strcpy(config.timeZone, tz.c_str());
    } else {
        Serial.printf("Time zone string too long; max length is %d.\n", sizeof(config.timeZone));
    }
}

/**
 * @brief The save ui command handler. Called by the ui object as needed.
 * 
 */
void onSave() {
    config.signature = CONFIG_SIG;
    saveConfig("Configuration saved.\n");
}

/**
 * @brief The restart ui command handler. Called by the ui object as needed.
 * 
 */
void onRestart() {
    Serial.print("Restarting.\n");
    ESP.restart();
}

/**
 * @brief The status ui command handler. Called by the ui object as needed.
 * 
 */
void onStatus(){
    if (running) {
        time_t nowSecs = time(nullptr);
        Serial.printf("The time is %s", ctime(&nowSecs));
        Serial.printf(
            "We're attached to WiFi SSID \"%s\".\n"
            "There our local IP address is ", 
            config.ssid);
        Serial.println(WiFi.localIP());
    }
    Serial.printf(
        "The web server is %srunning.\n"
        "The outlet is %s.\n"
        "The schedule is %s.\n",
        running ? "" : "not ", 
        digitalRead(RELAY) == RELAY_CLOSED ? "on" : "off", 
        config.enabled ? "enabled" : "disabled");
}

/**
 * @brief The Arduino setup function. Called once at power-on or reset.
 * 
 */
void setup() {
    // Do the basic hardware initialization.
    Serial.begin(9600);                 // Get Serial via UART up and running.
    delay(SERIAL_CONN_MILLIS);
    pinMode(LED, OUTPUT);               // Initialize the LED.
    digitalWrite(LED, LED_DARK);
    pinMode(RELAY, OUTPUT);             // Initialize the relay.
    digitalWrite(RELAY, RELAY_OPEN);
    button.begin();                     // Initialize the button.

    // Attach the ui command handlers
    ui.attachDefaultCmdHandler(onUnrecognized);
    if (!(
        ui.attachCmdHandler("help", onHelp) &&
        ui.attachCmdHandler("h", onHelp) &&
        ui.attachCmdHandler("ssid", onSsid) &&
        ui.attachCmdHandler("pw", onPw) &&
        ui.attachCmdHandler("tz", onTz) &&
        ui.attachCmdHandler("save", onSave) &&
        ui.attachCmdHandler("status", onStatus) &&
        ui.attachCmdHandler("restart", onRestart))
        ) {
        Serial.print("Couldn't attach all the ui command handlers.\n");
    }

    Serial.println(BANNER);             // Say hello.

    // See if we have our configuration data available
    EEPROM.begin(sizeof(eepromData_t));
    // If so try to get it from EEPROM.
    if (EEPROM.percentUsed() != -1) {
        EEPROM.get(0,config);
    }
    // If the configuration signature is not what we expect, we don't have credentials.
    running = config.signature == CONFIG_SIG;
    if (running) {
        // Get the WiFi connection going.
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
            // Set the system clock to the correct time
            running = setClock();
            wiFiServer.begin();
            // Get the webServer going and attach the HTTP method handlers.
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
            "Unable to get things up and running. Hopefully, the reason is clear.\n"
            "Use command line to set the WiFi credentials if needed./n"
            "Type \"help\" for help.\n");
    }
}

/**
 * @brief The Arduino loop function. Called repeatedly.
 * 
 */
void loop() {

    // Let the ui do its thing.
    ui.run();

    // Deal with button clicks.
    if (button.clicked()) {
            toggleOutlet();
    }

    // If the webServer is running, let it do its thing.
    if (running) {
        webServer.run();    // Let the web server do its thing
        followSchedule();   // Let the schedule follower do its thing
    }
}