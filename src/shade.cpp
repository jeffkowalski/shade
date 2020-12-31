#include <Arduino.h>
#include <EEPROM.h>
#include "Somfy_Remote.h"
#include <ESP8266WiFi.h>
#include "credentials.h"
#include "weenyMo.h"


static class Blinker {
    bool                enabled;
    int const           led = D4;
    unsigned long       previousMillis;   // will store last time LED was updated
    unsigned long const interval = 1000;  // interval at which to blink (milliseconds)

  public:
    Blinker (bool enable) : enabled (enable), previousMillis (0) {
        if (enabled)
            pinMode (led, OUTPUT);
    }

    void blink() const {
        if (!enabled)
            return;
        digitalWrite (led, LOW);
        delay (100);
        digitalWrite (led, HIGH);
        delay (100);
    }

    void update() {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
            blink();
            previousMillis = currentMillis;
        }
    }
} blinker (true);


#define INDICATOR D0

static SomfyRemote somfy ("somfy", 0x781413);  // <- Change remote name and remote code here!

static void commandShade (String command) {
    blinker.blink();
    Serial.println (command);
    somfy.move (command);
    EEPROM.commit();
    if (command[0] == 'U')               // off
        digitalWrite (INDICATOR, HIGH);  // ESP8266 builtin LED is "backwards" i.e. active LOW
    else if (command[0] == 'D')          // on
        digitalWrite (INDICATOR, LOW);   // ESP8266 builtin LED is "backwards" i.e. active LOW
}

//
// This function will be called when Alexa hears "switch [on | off] 'shade'"
//
static void onVoiceCommand (bool onoff) {
    Serial.printf ("onVoiceCommand %d\n", onoff);
    commandShade (onoff ? "DOWN" : "UP");
}

bool getAlexaState() {
    Serial.printf ("getAlexaState %d\n", !digitalRead (INDICATOR));
    return !digitalRead (INDICATOR);
}

static weenyMo weeny ("shade", onVoiceCommand);  // choose the name wisely: Alexa has very selective hearing!

void setup() {
    Serial.begin (9600);

    pinMode (INDICATOR, OUTPUT);
    digitalWrite (INDICATOR, HIGH);  // ESP8266 builtin LED is "backwards" i.e. active LOW

    blinker.blink();

    WiFi.begin (WIFI_SSID, WIFI_PSK);  // defined in credentials.h
    WiFi.waitForConnectResult();       // so much neater than those stupid loops and dots
    weeny.gotIPAddress();              // ready to roll...Tell Alexa to discover devices.

    blinker.blink();
    blinker.blink();

    Serial.println ("To program, long-press the program button on remote until blind goes up and down "
                    "slightly, then send 'PROGRAM' command via serial monitor.");
    Serial.println ("Ready");
}

void loop() {
    if (WiFi.status() != WL_CONNECTED)
        ESP.restart();

    // check if input is available
    if (Serial.available()) {
        String command = Serial.readString();
        command.trim();
        if (command[0] == 'U' || command[0] == 'D' || command[0] == 'M' || command[0] == 'P')
            commandShade (command);
    }

    blinker.update();
}
