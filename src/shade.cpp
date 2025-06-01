#include "Somfy_Remote.h"
#include "credentials.h"
#include "weenyMo.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>

static class Blinker {
    bool                enabled;
    int const           led = D4;
    unsigned long       previousMillis;   // will store last time LED was updated
    unsigned long const interval = 1000;  // interval at which to blink (milliseconds)

  public:
    Blinker (bool enable)
        : enabled (enable)
        , previousMillis (0) {
        if (enabled)
            pinMode (led, OUTPUT);
    }

    void blink () const {
        if (!enabled)
            return;
        digitalWrite (led, LOW);
        delay (100);
        digitalWrite (led, HIGH);
        delay (100);
    }

    void update () {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
            blink();
            previousMillis = currentMillis;
        }
    }
} blinker (true);

#define INDICATOR D0

// Declare pointer instead of static object to control initialization timing
static SomfyRemote * somfy = nullptr;

static void commandShade (const String & command) {  // Use const reference to avoid copying
    if (!somfy) {
        Serial.println ("ERROR: SomfyRemote not initialized");
        return;
    }

    if (command.length() == 0) {
        Serial.println ("ERROR: Empty command received");
        return;
    }

    Serial.printf ("commandShade called with: '%s'\n", command.c_str());

    blinker.blink();
    Serial.println (command);

    // Create a copy for the somfy library to avoid modifying the original
    String commandCopy = command;
    somfy->move (commandCopy);

    // Check first character safely
    char firstChar = command.charAt (0);  // Safer than command[0]
    if (firstChar == 'U') {               // off
        digitalWrite (INDICATOR, HIGH);   // ESP8266 builtin LED is "backwards" i.e. active LOW
        Serial.println ("INDICATOR set HIGH (UP)");
    }
    else if (firstChar == 'D') {        // on
        digitalWrite (INDICATOR, LOW);  // ESP8266 builtin LED is "backwards" i.e. active LOW
        Serial.println ("INDICATOR set LOW (DOWN)");
    }

    Serial.println ("commandShade completed");
}

//
// This function will be called when Alexa hears "switch [on | off] 'shade'"
//
static void onVoiceCommand (bool onoff) {
    Serial.printf ("onVoiceCommand %d\n", onoff);
    commandShade (onoff ? String ("DOWN") : String ("UP"));  // Explicit String construction
}

bool getAlexaState () {
    Serial.printf ("getAlexaState %d\n", !digitalRead (INDICATOR));
    return !digitalRead (INDICATOR);
}

static weenyMo weeny ("shade", onVoiceCommand);  // choose the name wisely: Alexa has very selective hearing!

static unsigned long boot_time;

void setup () {
    boot_time = millis();
    Serial.begin (9600);

    pinMode (INDICATOR, OUTPUT);
    digitalWrite (INDICATOR, HIGH);  // ESP8266 builtin LED is "backwards" i.e. active LOW

    blinker.blink();

    WiFi.begin (WIFI_SSID, WIFI_PSK);  // defined in credentials.h
    WiFi.waitForConnectResult();       // so much neater than those stupid loops and dots

    // Initialize SomfyRemote after EEPROM is ready
    somfy = new SomfyRemote ("somfy", 0x781413);
    delay (100);  // Give it time to initialize

    weeny.gotIPAddress();  // ready to roll...Tell Alexa to discover devices.

    blinker.blink();
    blinker.blink();

    Serial.println ("To program, long-press the program button on remote until blind goes up and down "
                    "slightly, then send 'PROGRAM' command via serial monitor.");
    Serial.println ("Ready");
}

void loop () {
    /* restart every 6 hours, to refresh from all manner of local network debacles */
    if ((millis() - boot_time) > (1000 * 60 * 60 * 6))  // 6 hours elapsed
        ESP.restart();

    if (WiFi.status() != WL_CONNECTED)
        ESP.restart();

    // check if input is available
    if (Serial.available()) {
        String command = Serial.readString();
        command.trim();
        if (command.length() > 0) {
            char firstChar = command.charAt (0);
            if (firstChar == 'U' || firstChar == 'D' || firstChar == 'M' || firstChar == 'P')
                commandShade (command);
        }
    }

    blinker.update();
}
