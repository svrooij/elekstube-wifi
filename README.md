# Elekstube WIFI

I've bought this [Elekstube clock](https://wiki.eleksmaker.com/doku.php?id=elekstube) on [Kickstarter](https://www.kickstarter.com/projects/938509544/elekstube-a-time-machine) which looks awesome, but is notoriously bad at timekeeping. Thankfully they prepared a serial connection on the internal board, with a way to set the time through that.

The [protocol](http://forum.eleksmaker.com/topic/1941/elekstube-api-control-protocol) is quite easy.

## Features

- [WiFiManager](https://github.com/tzapu/WiFiManager) to set your own wifi connection and set the time server and offset.
- Automatic NTP sync
- Configurable time offset
- Arduino over-the-air updates enabled
- **TODO** Add code to switch to config portal (either by a button or by a special url)

## Wemos D1 mini

I used a Wemos D1 mini that I had laying around, but any ESP8266 should do the trick. I've just cut a [dupont jumper cable](https://www.tinytronics.nl/shop/en/cables-and-connectors/cables-and-adapters/prototyping-wires/dupont-compatible-and-jumper/dupont-jumper-wire-female-female-10cm-10-wires) in half and soldered it directly to the board.

You can also add your own pin header if you like that.

Connections:

| Elekstube pin | Wemos D1 mini pin |
| --------------| ----------------- |
| `5V` | `5V` |
| `-` | `GND` |
| `RX` | `D4` |
| `TX` | not used |

## Needed libraries

To be able to build this app in the Arduino IDE, you'll need to install the following libraries from the library manager.

| Name | Version (tested) |  |
| -----| ---------------- | ---- |
| `ArduinoJSON` | `6.18.3` | [github](https://github.com/bblanchon/ArduinoJson) |
| `EspSoftwareSerial` | `6.13.2` | [github](https://github.com/plerup/espsoftwareserial) |
| `NTPClient` | `3.2.0` | [github](https://github.com/arduino-libraries/NTPClient) |
| `WiFiManager` | `2.0.3-alpha` | [github](https://github.com/tzapu/WiFiManager) |

## Protocol

If you're interested in how you can set the time over the serial connection.

The connection is just a regular serial connection with a baud rate of `115200` and it used *8 bits with 1 stop bit* commonly referred to as `8N1`.

### Set mode

Send one of these messages to set the mode:

- `$00` normal mode
- `$01` rainbow mode (changing colors for time)
- `$02` water fall mode (not a clue)
- `$03` test mode
- `$04` generate and show a random number

### Set time

This command will show this new time and **update** the internal clock of the Elekstube.

`*{leftDigitOfHour}{hexColor}{rightDigitOfHour}{hexColor}....`

So to set the time to `13:25:01` with the color orange (looks like the Nixie tubes) `402000`. You'll need to send the following message:

`*140200034020002402000540200004020001402000` or with spaces for readability (does not work) `*1 402000 3 402000 2 402000 5 402000 0 402000 1 402000`

### Show a number

To just show any 6 digit number send the following to the clock:

`/{firstDigit}{hexColor}{secondDigit}{hexColor}...`

## Inspiration

This project is inspired on [Elekstube by ib134866](https://github.com/ib134866/EleksTube), but I didn't want to setup a Blynk server or support a GPS chip.
