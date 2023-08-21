# WiFi Outlet

## Replacement firmware for the 2017 Sharper Image model 70011 WiFi controlled outlet

### About This Project

The goal of the project is to make firmware that presents the device as a web server on the local WiFi network. It presents web pages that enable it to be set up and controlled by connecting to it with a web browser. Quite what I want it do, I'm not clear.

As a proof of concept, the home page has a button on it that when clicked toggles the state of the outlet. The device's LED shows whether the outlet is on or off. The device's button also toggles the state of the outlet.

The implementation uses -- in addition to all the ESP WiFi stuff -- a super simple web server I wrote for the purpose. See SimpleWebServer.h for details.

### Notes on the Hardware

* The TYWE3S daughterboard in this device contains an ESP8266 microprocessor, SPI flash and some other stuff I haven't looked at. It exposes some of the ESP8266's pins. Enough to let us hack the device. The TYWE3S pin layout is as follows:

```
            <Antenna>
         Gnd         Vcc
      GPIO15         GPIO13
       GPIO2         GPIO12
       GPIO0         GPIO14
       GPIO4         GPIO16
       GPIO5         EN
        RXD0         ADC
        TXD0         RST
```

* GPIO0 is connected to one side of the button on the device. The other side is connected to Gnd. So, "active LOW."

* GPIO13 is connected to one side of the LED. The other side is connected, via a resistor to Vcc. So, the LED is "active LOW."

* GPIO14 is connected to the base of transistor Q1, the driver for the relay that turns the outlet on and off. It's "active HIGH." To turn the outlet on, hold GPIO14 HIGH.

* The other GPIOs and ADC aren't hooked to anything, so far as I know.

* To hack the device, you'll need to solder wires to Gnd, Vcc, RXD0, TXD0 and, for convenience, to GPIO0 and RST. Connect all but the last two of these to an FTDI serial-to-USB device. (Gnd --> GND, Vcc --> 3V3, RXD0 --> TXD, and TXD0 --> RXD). Put a female Dupont connector on the GPIO0 wire and a male one on the wire from RST. Find a place on the board connected to Gnd and solder a piece of wire to act as a header pin there. (The pads for the unoccupied R24 and R28 nearest the electrolytic capacitor worked for me.)

* To put the ESP8266 into "PGM from UART" mode, GPIO00 needs to be connected to Gnd. That can be done by attaching the wire from GPIO0 to the new header pin. Leaving GPIO0 floating results in the ESP8266 entering "Boot from SPI Flash mode." I.e., running normally.

* If the ESP8266 is "soft reset" in "PGM from UART" mode, which is what happens after new firmware is loaded into it, it will go into "Boot from SPI Flash" mode, even with GPIO0 attached to Gnd.

* To hardware reset the ESP8266, momentarily connect the wire from RST to Gnd.

### Copyright and License

Copyright (C) 2023 D.L. Ehnebuske

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
