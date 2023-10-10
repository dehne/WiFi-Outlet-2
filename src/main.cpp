/****
 * @file main.cpp
 * @version 1.5.0
 * @date September 28, 2023
 * 
 * WiFi Outlet -- Replacement firmware for the 2017 Sharper Image model 70011 WiFi 
 * controlled outlet. 
 * 
 * About this project
 * ==================
 * 
 * This sketch presents the Sharper Image model 70011 WiFi controlled outlet as a two-page web 
 * site on the local WiFi network. 
 * 
 * The home page shows two types of schedule for turning the outlet on and off at specified times 
 * of day. One schedule provides up to three on-off cycles per day. The other has one on-off cycle 
 * for weekdays and another for weekend days. Using the page you can set the times and which 
 * schedule to use. The page also lets you turn the outlet on and off manually.
 * 
 * The other page, /commandline.html, shows a "dumb terminal" with the same command line 
 * interface that's presented over the Serial interface. 
 * 
 * There's a button on the device. Clicking it toggles the outlet on or off.
 * 
 * The implementation uses -- in addition to all the ESP8266 WiFi stuff -- a super simple web 
 * server I wrote for the purpose. See SimpleWebServer.h for details. It also uses two other 
 * libraries I wrote for other projects, Commandline, which makes implementing a commandline 
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
#include <CommandLine.h>                            // My simple command line support library
#include <SimpleWebServer.h>                        // The web server library
#include <WebCmd.h>                                 // The "friend" extension of CommandLine for SimpleWebServer

//#define DEBUG                                       // Uncomment to enable debug code

// Pin definitions
#define LED             (13)                        // On the WiFi outlet PCB, this is active LOW
#define BUTTON          (0)                         // Active LOW
#define RELAY           (14)                        // On the WiFi outlet PCB, the relay that controls the outlet

// Misc constants
#define BANNER              "WiFi Switch V1.5.0"    // The tagline to identify this sketch
#define NTP_SERVER          "pool.ntp.org"          // The NTP server we use
#define TOGGLE_QUERY        "outlet=toggle"         // The URI query string to cause the outlet to toggle state
#define SCHED_UPDATE_QUERY  "schedule=update"       // The URI query string to cause the schedule parms to be updated
#define SCHED_TOGGLE_QUERY  "schedule=toggle"       // The URI query string to cause the schedule enable/disable toggle
#define CMD_SCREEN_LINES    (30)                    // The number of lines of text in the commandline.html "screen"
#define LED_LIT             (LOW)                   // digitalWrite value to light the LED
#define LED_DARK            (HIGH)                  // digitalWrite value to turn the LED off
#define RELAY_OPEN          (LOW)                   // digitlWrite value to open the relay
#define RELAY_CLOSED        (HIGH)                  // digitalWrite value to close the relay
#define OUTLET_ON           (true)                  // For setOutlet()
#define OUTLET_OFF          (false)                 // For setOutlet()
#define SERIAL_CONN_MILLIS  (4000)                  // millis() to wait after Serial.begin() before using it
#define WIFI_DELAY_MILLIS   (500)                   // millis() to delay between printing "." while waiting for WiFi and NTP
#define WIFI_CONN_MILLIS    (15000)                 // millis() to wait for WiFi connect before giving up
#define NTP_SET_MILLIS      (10000)                 // millis() to wait for NTP server to set the time
#define NOT_RUNNING_MINS    (5UL)                   // minutes to wait before restarting if internet not available
#define NOT_RUNNING_MILLIS  (NOT_RUNNING_MINS * 60000) // same as NOT_RUNNING_MINS but in micros()
#define DAWN_OF_HISTORY     (1533081600)            // Well, actually time_t for August 1st, 2018
#define MINS_PER_DAY        (1440)                  // Number of minutes in one day
#define CONFIG_SIG          (0x3B80)                // Our "signature" in EEPROM to know the data is (probably) ours

typedef unsigned int minPastMidnight_t;             // Minutes past midnight: 0 --> 1399
enum cycleType_t : uint8_t {daily, weekDay, weekEnd, _cycleTypeSize};   // The cycle types we support
#ifdef DEBUG
String cycleTypeName[_cycleTypeSize] = {"daily", "weekday", "weekend"};
#endif

struct eepromData_t {
    uint16_t signature;                             // Random integer identifying the data as ours. Change when shape changes
    char ssid[33];                                  // SSID of the WiFi network we should use.
    char password[64];                              // Password to use to connect to the WiFi
    char timeZone[32];                              // The POSIX timezone string for the timezone we use (see TZ.h)
    char outletName[32];                            // The name of the outlet
    bool enabled;                                   // If true, the schedule is enabled; if false schedule is disabled
    bool cycleEnable[3];                            // Whether each of the three on/off cycles is enabled
    cycleType_t cycleType[3];                       // The type of each of the three on/off cycles
    minPastMidnight_t cycleOnTime[3];               // The turn-on time for each of the three on/off cycles
    minPastMidnight_t cycleOffTime[3];              // The turn-off time for each of the three on/off cycles
    int cycleFuzz[3];                               // The number of minutes of randomness in each of the cycles
};

WiFiServer wiFiServer {80};                         // The WiFi server on port 80
SimpleWebServer webServer;                          // The web server object
PushButton button {BUTTON};                         // The PushButtone encapsulating the device's push button switch
CommandLine ui {};                                  // The command line interpreter object
WebCmd wc {&ui};                                    // The web command extension
String screenContents;                              // For the web command page, the screen contents

// The configuration we'll use, preset with default values
//                   sig ssid pw  ------- timezone -------  outletName  enabled --- cycleEnable ---
eepromData_t config {0,  "",  "", "PST8PDT,M3.2.0,M11.1.0", "McOutlet", false,  false, false, false, 
//                   ---- cycleType ----  -- cycleOnTime ---  -- cycleOffTime ----  - cycleFuzz -
                     daily, daily, daily, 8*60, 13*60, 19*60, 12*60, 17*60 , 21*60, 0,    0,    0};

// The field names in the home page's form. A POST request uses these to report form field values.
enum formDataName_t : uint8_t 
    {fdS0en, fdS1en, fdS2en, fdS0ty, fdS1ty, fdS2ty, fdS0on, fdS1on, fdS2on, fdS0of, fdS1of, fdS2of, fdS0fz, fdS1fz, fdS2fz, _formDataNameSize_};
String formDataNames[_formDataNameSize_] =
    {"s0en", "s1en", "s2en", "s0ty", "s1ty", "s2ty", "s0on", "s1on", "s2on", "s0of", "s1of", "s2of", "s0fz", "s1fz", "s2fz"};

unsigned long noWiFiMillis = 0;                     // millis() when we noticed the WiFi wasn't available; 0 otherwise
bool running = false;                               // True if we have a config, connect to WiFi and successfully set the time.
bool scheduleUpdated = true;                        // True when schedule updated since last looked at by followSchedule()

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
 * @brief Toggle the state of the LED
 * 
 */
inline void toggleLED() {
    digitalWrite(LED, digitalRead(LED) == LED_DARK ? LED_LIT : LED_DARK);
}

/**
 * @brief Set the LED to the specified state 
 * 
 * @param state     The state to set the LED to -- LED_DARK or LED_LIT
 */
inline void setLEDto(int state) {
    digitalWrite(LED, state);
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
    Serial.print(F("Waiting for NTP time sync..."));
    do {
        nowSecs = time(nullptr);
        Serial.print(".");
        toggleLED();
        delay(WIFI_DELAY_MILLIS);
    } while (nowSecs < DAWN_OF_HISTORY && millis() - startMillis < NTP_SET_MILLIS);
    if (nowSecs > DAWN_OF_HISTORY) {
        Serial.printf("Sync successful. Current time: %s", ctime(&nowSecs)); // ctime() appends a "\n", just because.
        setLEDto(LED_LIT);
        return true;
    } else {
        Serial.print("Unable to set the time.\n");
        setLEDto(LED_DARK);
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
    config.signature = CONFIG_SIG;
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
}

/**
 * @brief   Turn the outlet on or off as specified by outletOn value
 * 
 * @param outletOn  true ==> outlet turns on, false ==> outlet turns off
 */
void setOutletTo(bool outletOn) {
    digitalWrite(RELAY, outletOn ? RELAY_CLOSED : RELAY_OPEN);

    #ifdef DEBUG
    Serial.printf("  Turned outlet %s.\n", outletOn ? "on" : "off");
    #endif
}

/**
 * @brief Assemble the current state of our commandline page and send it to the httpClient. 
 * 
 * @param httpClient    The HTTP client we are to send the assembled page to.
 */
void sendCommandLinePage(WiFiClient* httpClient) {
    String pageHtml =   "<!doctype html>\n"
                    "<html>\n"
                    "<head>\n"
                    "<meta charset=\"utf-8\">\n"
                    "<title>WiFi Outlet Command Processor</title>\n"
                    "<style>\n"
                    "body {\n"
                    "background-color: black;\n"
                    "color: antiquewhite;\n"
                    "font-family: \"Gill Sans\", \"Gill Sans MT\", \"Myriad Pro\", \"DejaVu Sans Condensed\", Helvetica, Arial, \"sans-serif\";\n"
                    "}\n"
                    "h1 {\n"
                    "text-align: center;\n"
                    "font-family: Cambria, \"Hoefler Text\", \"Liberation Serif\", Times, \"Times New Roman\", \"serif\";\n"
                    "}\n"
                    ".screen {\n"
                    "font-family: Consolas, \"Andale Mono\", \"Lucida Console\", \"Lucida Sans Typewriter\", Monaco, \"Courier New\", \"monospace\";\n"
                    "font-size: 12pt;\n"
                    "color: lightgreen;\n"
                    "}\n"
                    "textarea {\n"
                    "background-color: black;\n"
                    "font-family: Consolas, \"Andale Mono\", \"Lucida Console\", \"Lucida Sans Typewriter\", Monaco, \"Courier New\", \"monospace\";\n"
                    "font-size: 12pt;\n"
                    "color: lightgreen;\n"
                    "border-style: none;\n"
                    "}\n"
                    "input {\n"
                    "background-color: black;\n"
                    "font-family: Consolas, \"Andale Mono\", \"Lucida Console\", \"Lucida Sans Typewriter\", Monaco, \"Courier New\", \"monospace\";\n"
                    "font-size: 12pt;\n"
                    "color: lightgreen;\n"
                    "border-style: none;\n"
                    "}\n"
                    "input:focus {\n"
                    "outline: none!important\n"
                    "}\n"
                    "</style>\n"
                    "</head>\n"
                    "<body>\n"
                    "<h1>WiFi Outlet &ldquo;@outletName&rdquo; Command Processor</h1>\n"
                    "<p>Using this page you can interact with the Outlet's command processor.</p>\n"
                    "<form method=\"post\">\n"
                    "<textarea class=\"screen\" name=\"screen\" cols=\"120\" rows=\"@rows\" tabindex=\"0\">\n"
                    "@display\n"
                    "</textarea><br />\n"
                    "<span class=\"screen\">@prompt </span><input type=\"text\" name=\"cmd\" maxlength=\"120\" size=\"120\" tabindex=\"0\" autofocus />\n"
                    "<input type=\"submit\" tabindex=\"0\" hidden />\n"
                    "</form>\n"
                    "<p>&nbsp;</p>\n"
                    "<p style=\"font-size: 80%\" >@outletBanner Copyright &copy; 2023 by D. L. Ehnebuske.</p>\n"
                    "</body>\n"
                    "</html>\r\n"
                    "\r\n";
    pageHtml.replace("@outletName", config.outletName);
    pageHtml.replace("@rows", String(CMD_SCREEN_LINES));
    pageHtml.replace("@display", screenContents);
    pageHtml.replace("@prompt", CMD_PROMPT);
    pageHtml.replace("@outletBanner", BANNER);

    httpClient->print(pageHtml);
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
                        "font-family: \"Gill Sans\", \"Gill Sans MT\", \"Myriad Pro\", \"DejaVu Sans Condensed\", Helvetica, Arial, \"sans-serif\";\n"
                        "}\n"
                        "h1 {\n"
                        "text-align: center;\n"
                        "font-family: Cambria, \"Hoefler Text\", \"Liberation Serif\", Times, \"Times New Roman\", \"serif\";\n"
                        "}\n"
                        "td {\n"
                        "text-align: center;\n"
                        "}\n"
                        ".hdr {\n"
                        "background-color: #3A3A3A;\n"
                        "}\n"
                        "</style>\n"
                        "</head>\n"
                        "<body>\n"
                        "<h1>WiFi Outlet &ldquo;@outletName&rdquo; Control Panel</h1>\n"
                        "<p>Using this page you can set up and control your WiFi outlet. You can schedule up to three on-off cycles. For each cycle, you can set it to happen every day, only on weekdays or only on weekend days. For each enabled cycle, the outlet will turn on at the &ldquo;From&rdquo; time and turn off at the &ldquo;to&rdquo; time. Setting the &ldquo;+/-&rdquo; value to something other than zero will make the on and off times vary randomly by up to the specified number of minutes. Once you have set up the schedule, click on the &ldquo;Save schedule&rdquo; button to save it in the outlet and put it into effect.</p>\n"
                        "<p>Using the buttons, you can also turn schedule-following on and off and manually turn the outlet on and off.</p>\n"
                        "<form method=\"post\">\n"
                        "<h2>Schedule</h2>\n"
                        "<table width=\"80%\" border=\"0\" cellpadding=\"10\">\n"
                        "<tbody>\n"
                        "<tr>\n"
                        "<td class=\"hdr\">\n"
                        "<input type=\"checkbox\" name=\"s0en\" @s0en>\n"
                        "<label for=\"s0en\">Enable</label>\n"
                        "</td>\n"
                        "<td class=\"hdr\">\n"
                        "<input type=\"checkbox\" name=\"s1en\" @s1en>\n"
                        "<label for=\"s1en\">Enable</label>\n"
                        "</td>\n"
                        "<td class=\"hdr\">\n"
                        "<input type=\"checkbox\" name=\"s2en\" @s2en>\n"
                        "<label for=\"s2en\">Enable</label>\n"
                        "</td>\n"
                        "</tr>\n"
                        "<tr>\n"
                        "<td>\n"
                        "<input type=\"radio\" name=\"s0ty\" value=\"s0dy\" @s0dy>\n"
                        "<label for=\"s0dy\">Daily</label><br/>\n"
                        "<input type=\"radio\" name=\"s0ty\" value=\"s0wd\" @s0wd>\n"
                        "<label for=\"s0wd\">Weekday</label>   \n"
                        "<input type=\"radio\" name=\"s0ty\" value=\"s0we\" @s0we>\n"
                        "<label for=\"s0we\">Weekend</label>\n"
                        "</td>\n"
                        "<td>\n"
                        "<input type=\"radio\" name=\"s1ty\" value=\"s1dy\" @s1dy>\n"
                        "<label for=\"s1dy\">Daily</label><br/>\n"
                        "<input type=\"radio\" name=\"s1ty\" value=\"s1wd\" @s1wd>\n"
                        "<label for=\"s1wd\">Weekday</label>\n"
                        "<input type=\"radio\" name=\"s1ty\" value=\"s1we\" @s1we>\n"
                        "<label for=\"s1we\">Weekend</label>\n"
                        "</td>\n"
                        "<td>\n"
                        "<input type=\"radio\" name=\"s2ty\" value=\"s2dy\" @s2dy>\n"
                        "<label for=\"s2dy\">Daily</label><br/>\n"
                        "<input type=\"radio\" name=\"s2ty\" value=\"s2wd\" @s2wd>\n"
                        "<label for=\"s2wd\">Weekday</label>\n"
                        "<input type=\"radio\" name=\"s2ty\" value=\"s2we\" @s2we>\n"
                        "<label for=\"s2we\">Weekend</label>\n"
                        "</td>\n"
                        "</tr>\n"
                        "<tr>\n"
                        "<td>\n"
                        "From\n"
                        "<input type=\"time\" name=\"s0on\" value=\"@s0on\">\n"
                        "to\n"
                        "<input type=\"time\" name=\"s0of\" value=\"@s0of\"><br/>\n"
                        "+/-\n"
                        "<input type=\"number\" name=\"s0fz\" value=\"@s0fz\">\n"
                        "min\n"
                        "</td>\n"
                        "<td>\n"
                        "From\n"
                        "<input type=\"time\" name=\"s1on\" value=\"@s1on\">\n"
                        "to\n"
                        "<input type=\"time\" name=\"s1of\" value=\"@s1of\"><br/>\n"
                        "+/-\n"
                        "<input type=\"number\" name=\"s1fz\" value=\"@s1fz\">\n"
                        "min\n"
                        "</td>\n"
                        "<td>\n"
                        "From\n"
                        "<input type=\"time\" name=\"s2on\" value=\"@s2on\">\n"
                        "to\n"
                        "<input type=\"time\" name=\"s2of\" value=\"@s2of\"><br/>\n"
                        "+/-\n"
                        "<input type=\"number\" name=\"s2fz\" value=\"@s2fz\">\n"
                        "min\n"
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
                        "<p>&nbsp;</p>\n"
                        "<p style=\"font-size: 80%\" >@outletBanner Copyright &copy; 2023 by D. L. Ehnebuske.</p>\n"
                        "</body>\n"
                        "</html>\r\n"
                        "\r\n";

    // Substitute all the variable information needed in the HTLM. Each variable in the text is a name beginning with "@"
    pageHtml.replace("@outletName", config.outletName);
    pageHtml.replace("@s0en", config.cycleEnable[0] ? "checked" : "");
    pageHtml.replace("@s1en", config.cycleEnable[1] ? "checked" : "");
    pageHtml.replace("@s2en", config.cycleEnable[2] ? "checked" : "");
    pageHtml.replace("@s0dy", config.cycleType[0] == daily ? "checked" : "");
    pageHtml.replace("@s0wd", config.cycleType[0] == weekDay ? "checked" : "");
    pageHtml.replace("@s0we", config.cycleType[0] == weekEnd ? "checked" : "");
    pageHtml.replace("@s1dy", config.cycleType[1] == daily ? "checked" : "");
    pageHtml.replace("@s1wd", config.cycleType[1] == weekDay ? "checked" : "");
    pageHtml.replace("@s1we", config.cycleType[1] == weekEnd ? "checked" : "");
    pageHtml.replace("@s2dy", config.cycleType[2] == daily ? "checked" : "");
    pageHtml.replace("@s2wd", config.cycleType[2] == weekDay ? "checked" : "");
    pageHtml.replace("@s2we", config.cycleType[0] == weekEnd ? "checked" : "");
    pageHtml.replace("@s0on", fromMinsPastMidnight(config.cycleOnTime[0]));
    pageHtml.replace("@s0of", fromMinsPastMidnight(config.cycleOffTime[0]));
    pageHtml.replace("@s1on", fromMinsPastMidnight(config.cycleOnTime[1]));
    pageHtml.replace("@s1of", fromMinsPastMidnight(config.cycleOffTime[1]));
    pageHtml.replace("@s2on", fromMinsPastMidnight(config.cycleOnTime[2]));
    pageHtml.replace("@s2of", fromMinsPastMidnight(config.cycleOffTime[2]));
    pageHtml.replace("@s0fz", String(config.cycleFuzz[0]));
    pageHtml.replace("@s1fz", String(config.cycleFuzz[1]));
    pageHtml.replace("@s2fz", String(config.cycleFuzz[2]));
    pageHtml.replace("@schedIs", config.enabled ? "enabled" : "disabled");
    pageHtml.replace("@schedWillBe", config.enabled ? "disable" : "enable");
    pageHtml.replace("@schEnButton", config.enabled ? "Disable" : "Enable");
    pageHtml.replace("@outletIs", outletIsOn() ? "on" : "off");
    pageHtml.replace("@outletWillBe", outletIsOn() ? "off" : "on");
    pageHtml.replace("@outletBanner", BANNER);

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

    // We have a "home page." If that's what was asked for, proceed.
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
    } else if (trPath.equals("/commandline.html") || (trPath.equals("/commandline.htm")) ) {
        httpClient->print(swsNormalResponseHeaders);
        if (webServer->httpMethod() == swsGET) {
            sendCommandLinePage(httpClient);
        #ifdef DEBUG
            Serial.print("GET request for commandline page received and processed.\n");
            ui.cancelCmd();
        } else {
            Serial.print("HEAD request for commandline page received and processed.\n");
            ui.cancelCmd();
        #endif
        }

    // Otherwise, they asked for some other resource. We don't have it. Respond with "404 Not Found" message.
    } else {
        httpClient->print(swsNotFoundResponse);
        Serial.print("GET or HEAD request received for some page we don't have. Sent \"404 not found\"\n");
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
            Serial.printf("[handlePost] Update schedule. Message headers: \"%s\"\nForm data: ", webServer->clientHeaders().c_str());
            #endif
            config.cycleEnable[0] = config.cycleEnable[1] = config.cycleEnable[2] = false; // N.B. Only sent in POST data when "on"
            for (uint8_t i = 0; i < _formDataNameSize_; i++) {
                String formValue = webServer->getFormDatum(formDataNames[i]);
                if (formValue.length() != 0) {
                    #ifdef DEBUG
                    Serial.printf("%s = \"%s\" ", formDataNames[i].c_str(), formValue.c_str());
                    #endif
                    switch ((formDataName_t)i) {
                        case fdS0en:
                            config.cycleEnable[0] = formValue == "on" ? true : false;
                            break;
                        case fdS1en:
                            config.cycleEnable[1] = formValue == "on" ? true : false;
                            break;
                        case fdS2en:
                            config.cycleEnable[2] = formValue == "on" ? true : false;
                            break;
                        case fdS0ty:
                            config.cycleType[0] = formValue == "s0dy" ? daily : formValue == "s0wd" ? weekDay : weekEnd;
                            break;
                        case fdS1ty:
                            config.cycleType[1] = formValue == "s1dy" ? daily : formValue == "s1wd" ? weekDay : weekEnd;
                            break;
                        case fdS2ty:
                            config.cycleType[2] = formValue == "s2dy" ? daily : formValue == "s2wd" ? weekDay : weekEnd;
                            break;
                        case fdS0on:
                            config.cycleOnTime[0] = toMinsPastMidnight(formValue);
                            break;
                        case fdS1on:
                            config.cycleOnTime[1] = toMinsPastMidnight(formValue);
                            break;
                        case fdS2on:
                            config.cycleOnTime[2] = toMinsPastMidnight(formValue);
                            break;
                        case fdS0of:
                            config.cycleOffTime[0] = toMinsPastMidnight(formValue);
                            break;
                        case fdS1of:
                            config.cycleOffTime[1] = toMinsPastMidnight(formValue);
                            break;
                        case fdS2of:
                            config.cycleOffTime[2] = toMinsPastMidnight(formValue);
                            break;
                        case fdS0fz:
                            config.cycleFuzz[0] = formValue.toInt();
                            break;
                        case fdS1fz:
                            config.cycleFuzz[1] = formValue.toInt();
                            break;
                        case fdS2fz:
                            config.cycleFuzz[2] = formValue.toInt();
                            break;
                        default:
                            break;
                    }
                }
            }
            // Save the new data in config to EEPROM
            #ifdef DEBUG
            Serial.print("\n");
            saveConfig("[handlePost] Configuration update saved.\n");
            ui.cancelCmd();
            #else
            saveConfig();
            #endif
            scheduleUpdated = true;     // Let followSchedule() know we've updated the schedule 
            
        // Deal with a query to home page that we don't understand
        } else {
            Serial.printf("POST request received for query we don't understand: \"%s\".\n", trQuery.c_str());
            Serial.printf(" Client message body: \"%s\".\n", webServer->clientBody().c_str());
            ui.cancelCmd(); // Reissue command prompt after print
            httpClient->print(swsBadRequestResponse);
            return;
        }

        // Tell client we're good and go look at the home page for the result.
        httpClient->print("HTTP/1.1 303 See other\r\n"
                          "Location: /index.html\r\n\r\n");
        return;

    // Deal with a POST to the commandline page.
    } else if (trPath.equals("/commandline.html") || trPath.equals("/commandline.htm")) {

        // Retrieve the command and the current contents of the "screen"
        String cmdLine = webServer->getFormDatum("cmd");
        screenContents = webServer->getFormDatum("screen");

        // From the screenContents remove all "\r" chars and trailing second "\n", if present.
        screenContents.replace("\r", "");
        if (screenContents.endsWith("\n\n")) {
            screenContents.remove(screenContents.length() - 1);
        }

        // Execute the command and construct the line(s) we'll display on the screen.
        String cmdResult = String(CMD_PROMPT) + cmdLine + "\n" + wc.doCommand(cmdLine);

        // Count the number of lines in the result
        int16_t resultLines = 1;
        int16_t ix = 0;
        while ((ix = cmdResult.indexOf("\n", ix) + 1) <= cmdResult.lastIndexOf("\n")) {
            resultLines++;
        }

        // Remove trailing "\n", if any.
        if (cmdResult.endsWith("\n")) {
            cmdResult.remove(cmdResult.length() - 1);
        }

        // As needed, remove lines from the start of the result to get the line count below what fits on the screen.
        while (resultLines > CMD_SCREEN_LINES) {
            cmdResult = cmdResult.substring(cmdResult.indexOf("\n") + 1);
            resultLines--;
        }

        // Count the lines in the current screenContents.
        int16_t screenContentLines = 1;
        ix = 0;
        while ((ix = screenContents.indexOf("\n", ix) + 1) < screenContents.lastIndexOf("\n")) {
            screenContentLines++;
        }
        
        // As needed, remove lines from the start of screenContents to get the line count below what
        // together with cmdResult fits on the screen. Then add the new results to the end.
        while (screenContentLines + resultLines > CMD_SCREEN_LINES) {
            screenContents = screenContents.substring(screenContents.indexOf("\n") + 1);
            screenContentLines --;
        }
        screenContents += cmdResult;

        // Tell the client we're good and to go look at the page again for the result.
        httpClient->print("HTTP/1.1 303 See other\r\n"
                          "Location: /commandline.html\r\n\r\n");
        return;
    }
    // The client POSTed to a page we can't deal with. Respond with "400 Bad Request" message.
    Serial.printf("POST request received for something other than the home page. path: \"%s\" query: \"%s\".\n",
        trPath.c_str(), trQuery.c_str());
    Serial.printf(" Client message body: \"%s\".\n", webServer->clientBody().c_str());
    ui.cancelCmd(); // Reissue command prompt after print
    httpClient->print(swsBadRequestResponse);
}

/**
 * @brief Utility function to follow the schedule defined by config. 
 * 
 */
void followSchedule() {
    static minPastMidnight_t lastMinPastMidnight = 0;       // When we last noticed the time change.
    // config.cycleOn and config.cycleOff times adjusted by cycleFuzz
    static minPastMidnight_t cycleOn[sizeof(config.cycleEnable) / sizeof(config.cycleEnable[0])];
    static minPastMidnight_t cycleOff[sizeof(config.cycleEnable) / sizeof(config.cycleEnable[0])];
    time_t curTime = time(nullptr);
    struct tm *timeval;
    timeval = localtime(&curTime);
    minPastMidnight_t curMinPastMidnight = timeval->tm_hour * 60 + timeval->tm_min;

    // If the schedule is turned off, or the time hasn't changed and there's no new schedule, nothing to do
    if (!config.enabled || (curMinPastMidnight == lastMinPastMidnight && !scheduleUpdated)) {
        return;
    }

    // Handle an updated schedule by using the configured on/off times, initially with no cycleFuzz
    if (scheduleUpdated) {
        for (uint8_t c = 0; c < sizeof(config.cycleEnable) / sizeof(config.cycleEnable[0]); c++){
            cycleOn[c] = config.cycleOnTime[c];
            cycleOff[c] = config.cycleOffTime[c];
        }
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

    bool isWeekday = (timeval->tm_wday >= 1 && timeval->tm_wday <= 5);
    // Handle each of the on/off cycles
    for (uint8_t c = 0; c < sizeof(config.cycleEnable) / sizeof(config.cycleEnable[0]); c++){
        #ifdef DEBUG
        Serial.printf("[followSchedule] Cycle %d is %s.\n", c, config.cycleEnable[c] ? "enabled" : "disabled");
        #endif
        // If cycle c is enabled and applicable today
        if (config.cycleEnable[c] && 
            (config.cycleType[c] == daily || 
            (config.cycleType[c] == weekDay && isWeekday) || 
            (config.cycleType[c] == weekEnd && !isWeekday))) {

            // If it's midnight and the fuzzy cycleOn[c] time was suspended, put it into effect now
            if (curMinPastMidnight == 0 && cycleOn[c] >= MINS_PER_DAY) {
                cycleOn[c] -= MINS_PER_DAY;
            }
            // If it's time for this cycleOn to go into effect, turn the outlet on and, if needed, calculate a new cycleOn
            if (cycleOn[c] == curMinPastMidnight) {
                setOutletTo(OUTLET_ON);
                // If the cycleOn is fuzzy, figure out what the new on time should be
                if (config.cycleFuzz[c] != 0) {
                    // If switch off comes later in the day, the new on time is config on time +/- a random amount <= the fuzz.
                    if (config.cycleOnTime[c] < config.cycleOffTime[c]) {
                        int randMax = config.cycleFuzz[c] >= 0 ? config.cycleFuzz[c] : -config.cycleFuzz[c];
                        cycleOn[c] = (config.cycleOnTime[c] + random(2 * randMax) - randMax + MINS_PER_DAY) % MINS_PER_DAY;
                        // If we'd run into the new time later today, suspend the new value until midnight
                        if (cycleOn[c] > curMinPastMidnight) {
                            cycleOn[c] += MINS_PER_DAY;
                        }
                    // Otherwise, the the new on time is the config on time + whatever fuzz the off time is using
                    } else {
                        cycleOn[c] = config.cycleOnTime[c] + cycleOff[c] - config.cycleOffTime[c];
                    }
                    #ifdef DEBUG
                    Serial.printf("[followSchedule] New on time for cycle %d: %s%s.\n", 
                      c, fromMinsPastMidnight(cycleOn[c] % MINS_PER_DAY).c_str(), cycleOn[c] >= MINS_PER_DAY ? " (suspended)" : "");
                    #endif
                }
            }

            // If it's midnight and the fuzzy cycleOff[c] time was suspended, put it into effect now
            if (curMinPastMidnight == 0 && cycleOff[c] >= MINS_PER_DAY) {
                cycleOff[c] -= MINS_PER_DAY;
            }

            // If it's time for this cycleOoff to go into effect, turn the outlet off and calculate a new, fuzzy cycleOff
            if (cycleOff[c] == curMinPastMidnight) {
                setOutletTo(OUTLET_OFF);
                // If the cycleOff is fuzzy, figure out what the new off time should be
                if( config.cycleFuzz[c] != 0) {
                    // if the off time follows the on time, use the same fuzz the the on time figured out
                    if (config.cycleOnTime[c] < config.cycleOffTime[c]) {
                        cycleOff[c] = config.cycleOffTime[c] + cycleOn[c] - config.cycleOnTime[c];
                    // Otherwise use the configured off time +/- a random amount <= the fuzz.
                    } else {
                        int randMax = config.cycleFuzz[c] >= 0 ? config.cycleFuzz[c] : -config.cycleFuzz[c];
                        cycleOff[c] = (config.cycleOffTime[c] + random(2 * randMax) - randMax + MINS_PER_DAY) % MINS_PER_DAY;
                        // If we'd run into the new time later today, suspend the new value until midnight
                        if (cycleOff[c] > curMinPastMidnight) {
                            cycleOff[c] += MINS_PER_DAY;
                        }
                    }
                    #ifdef DEBUG
                    Serial.printf("[followSchedule] New off time for cycle %d: %s%s.\n", 
                      c, fromMinsPastMidnight(cycleOff[c] % MINS_PER_DAY).c_str(), cycleOff[c] >= MINS_PER_DAY ? " (suspended)" : "");
                    #endif
                }
            }
        }
    }

    lastMinPastMidnight = curMinPastMidnight;               // We handled things for the current time
    #ifdef DEBUG
    ui.cancelCmd();
    #endif
}

/**
 * @brief The "help" and "h" ui command handler. Called by ui object as needed.
 * 
 */
String onHelp(CommandHandlerHelper* helper) {
   return 
        "Help for " BANNER "\n"
        "  help             Print this text\n"
        "  h                Same as 'help'\n"
        "  ssid [<ssid>]    Print or set the ssid of the WiFi AP we should connect to\n"
        "  pw [<password>]  Print or set the password we are to use to connect\n"
        "  tz [<timezone>]  Print or set the POSIX time zone string for the time zone we are in\n"
        "  name [<name>]    Print or set the outlet's name\n"
        "  save             Save the current ssid and password and continue\n"
        "  status           Print the status of the system\n"
        "  restart          Restart the device. E.g., to use newly saved WiFi credentials.\n";
}

/**
 * @brief The ssid ui command handler. Called by the ui object as needed.
 * 
 */
String onSsid(CommandHandlerHelper* helper) {
    String ssid = helper->getCommandLine().substring(helper->getWord(0).length());
    ssid.trim();
    unsigned int ssidLen = ssid.length();
    if (ssidLen != 0 && ssidLen < sizeof(config.ssid)) {
        strcpy(config.ssid, ssid.c_str());
        return String("SSID changed to \"" + ssid + "\"\n");
    } else if (ssidLen == 0) {
        return String("SSID is \"") + String(config.ssid) + "\"\n";
    }
    return String("Specified SSID is too long. Maximum length is ") + String(sizeof(config.ssid) - 1) + "\n";
}

/**
 * @brief The pw ui command handler. Called by the ui object as needed.
 * 
 */
String onPw(CommandHandlerHelper* helper) {
    String pw = helper->getCommandLine().substring(helper->getWord(0).length());
    pw.trim();
    unsigned int pwLen = pw.length();
    if (pwLen > 0 && pwLen < sizeof(config.password)) {
        strcpy(config.password, pw.c_str());
        return String("Password changed to \"") + pw + "\"\n";
    } else if (pwLen == 0) {
        return String("Password is \"") + String(config.password) + "\"\n";
    }
    return String("Password is too long. Maximum length is ") + String(sizeof(config.password) - 1) + ".\n";
}

/**
 * @brief The tz ui command handler. Called by the ui object as needed.
 * 
 */
String onTz(CommandHandlerHelper* helper) {
    String tz = helper->getWord(1);
    if (tz.length() == 0) {
        return "Timezone is \"" + String(config.timeZone) + "\".\n";
    } else if (tz.length() < sizeof(config.timeZone)) {
        strcpy(config.timeZone, tz.c_str());
        return String("Timezone changed to \"") + String(config.outletName) + "\".\n";
    }
    return String("Time zone string too long; max length is ") + String(sizeof(config.timeZone)) + ".\n";
}

/**
 * @brief The name ui command handler. Called by the ui object as needed.
 * 
 */
String onName(CommandHandlerHelper* helper) {
    String name = helper->getCommandLine().substring(helper->getWord(0).length());
    name.trim();
    unsigned int nameLen = name.length();
    if (nameLen != 0 && nameLen < sizeof(config.outletName)) {
        strcpy(config.outletName, name.c_str());
        return String("Outlet name changed to \"") + name + "\"\n";
    } else if (nameLen == 0) {
        return String("Outlet name is \"") + String(config.timeZone) + "\"\n";
    }
    return String("The specified outlet name is too long. Maximum length is ") + String(sizeof(config.outletName) - 1) + "\n";
}

/**
 * @brief The save ui command handler. Called by the ui object as needed.
 * 
 */
String onSave(CommandHandlerHelper* helper) {
    config.signature = CONFIG_SIG;
    saveConfig();
    return "Configuration saved.\n";
}

/**
 * @brief The restart ui command handler. Called by the ui object as needed.
 * 
 */
String onRestart(CommandHandlerHelper* helper) {
    ESP.restart();
    return "";      // The compiler doesn't know restart never returns
}

/**
 * @brief The status ui command handler. Called by the ui object as needed.
 * 
 */
String onStatus(CommandHandlerHelper* helper){
    String answer = "";
    if (running) {
        time_t nowSecs = time(nullptr);
        answer +=   "The time is " + String(ctime(&nowSecs)) + 
                    "We're attached to WiFi SSID \"" + String(config.ssid) + "\".\n" +
                    "There our local IP address is " + WiFi.localIP().toString() + ".\n";
    }
    answer +=   String("The web server is " + String(running ? "" : "not ") + "running.\n") +
                "The outlet is " + String(digitalRead(RELAY) == RELAY_CLOSED ? "on" : "off") + ".\n"
                "The schedule is " + String(config.enabled ? "enabled" : "disabled") + ".\n";
    return answer;
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
    setLEDto(LED_DARK);
    pinMode(RELAY, OUTPUT);             // Initialize the relay.
    digitalWrite(RELAY, RELAY_OPEN);
    button.begin();                     // Initialize the button.

    // Attach the ui command handlers
    if (!(
        ui.attachCmdHandler("help", onHelp) &&
        ui.attachCmdHandler("h", onHelp) &&
        ui.attachCmdHandler("ssid", onSsid) &&
        ui.attachCmdHandler("pw", onPw) &&
        ui.attachCmdHandler("tz", onTz) &&
        ui.attachCmdHandler("name", onName) &&
        ui.attachCmdHandler("save", onSave) &&
        ui.attachCmdHandler("status", onStatus) &&
        ui.attachCmdHandler("restart", onRestart))
        ) {
        Serial.print("Couldn't attach all the ui command handlers.\n");
    }

    Serial.println(BANNER);                 // Say hello.
    screenContents = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n" + 
                    String(BANNER) + "\n"
                    "Type \"help\" for a list of commands.\n"; // Also in the web commandline page

    // See if we have our configuration data available
    EEPROM.begin(sizeof(eepromData_t));
    // If so try to get it from EEPROM.
    eepromData_t storedConfig;
    if (EEPROM.percentUsed() != -1) {
        EEPROM.get(0,storedConfig);
    }
    #ifdef DEBUG
    Serial.printf("Got stored data. signature: 0x%x, ssid: %s.\n", storedConfig.signature, storedConfig.ssid);
    #endif
    // If the stored signature matches, assume the stored data is our config
    if (storedConfig.signature == CONFIG_SIG) {
        config = storedConfig;
    }
    // If there's an SSID and password, presume we'll get up and going
    running = config.ssid[0] != '\0' && config.password[0] != '\0';
    noWiFiMillis = 0;
    if (running) {
        // Get the WiFi connection going.
        Serial.printf("\nConnecting to %s ", config.ssid);
        unsigned long startMillis = millis();
        WiFi.begin(config.ssid, config.password);
        while (WiFi.status() != WL_CONNECTED && millis() - startMillis < WIFI_CONN_MILLIS) {
            delay(WIFI_DELAY_MILLIS);
            Serial.print(".");
            toggleLED();
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
            setLEDto(LED_LIT);
        } else {
            Serial.printf("Unable to connect to WiFi. Status: %d\n", WiFi.status());
            running = false;                    // We're not running
        }
        // If we should have been able to connect and get the time, but couldn't note when it happened
        if (!running) {
            Serial.printf("Expected to connect to WiFi and set the time, but couldn't. Will try again in %ld minutes.\n", 
                NOT_RUNNING_MINS);
            noWiFiMillis = millis();
        } else {
            randomSeed((unsigned long)(time(nullptr) && 0xFFFFFFFF));
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
    unsigned long curMillis = millis();     // millis() now

    // Let the ui do its thing.
    ui.run();

    // Deal with button clicks: toggle outlet.
    if (button.clicked()) {
            toggleOutlet();
    }

    // Deal with button long presses: reset and, because the button is down, enter "PGM from UART" mode.
    if (button.longPressed()) {
        Serial.print("Resetting for firmware update.\n");
        setLEDto(LED_DARK);
        ESP.reset();
    }

    // If everything should be up and running,
    if (running) {
        // If the WiFi is still connected
        if (WiFi.status() == WL_CONNECTED) {
            webServer.run();                    // Let the web server do its thing
            followSchedule();                   // Let the schedule follower do its thing
            noWiFiMillis = 0;                   // We do have an internet connection (perhaps reacquired)

        // Otherwise, the WiFi connection we had isn't there anymore. If this the first we saw that
        } else if (noWiFiMillis == 0) {
            Serial.printf("Oops! The WiFi connection seems to have disappeared. Will try to reconnect in %ld minutes.\n",
                NOT_RUNNING_MINS);
            noWiFiMillis = curMillis;           // Note when we first noticed there was no internet connection
        }
    }
    
    // If it looks like the internet is configured but hasn't been available for some time
    if (noWiFiMillis != 0 && curMillis - noWiFiMillis > NOT_RUNNING_MILLIS && config.ssid[0] != '\0' && config.password[0] != '\0') {
        Serial.print("Restarting to see if the WiFi is back.\n");
        ESP.restart();                          // Try restarting
    }
}