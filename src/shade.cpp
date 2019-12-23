#include "credentials.h"
#include <EEPROM.h>
#include <Somfy_Remote.h>
#include <weenyMo.h>

#define EEPROM_SIZE 64
//#define INDICATOR LED_BUILTIN
#define INDICATOR D4

SomfyRemote somfy("remote1", 0x131478); // <- Change remote name and remote code here!

class Blinker {
  bool enabled;
  const int led = D4;
  unsigned long previousMillis;        // will store last time LED was updated
  const unsigned long interval = 1000; // interval at which to blink (milliseconds)

public:
  Blinker(bool enable) : enabled(enable), previousMillis(0) {
    if (enabled)
      pinMode(led, OUTPUT);
  }

  void blink() {
    if (!enabled)
      return;
    digitalWrite(led, LOW);
    delay(100);
    digitalWrite(led, HIGH);
    delay(100);
  }

  void update() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      // Serial.println("blink");
      blink();
      previousMillis = currentMillis;
    }
  }
} Blinker(false);

void commandShade(String command) {
  Blinker.blink();
  Serial.println(command);
  somfy.move(command);
  EEPROM.commit();
  if (command == "UP")               // off
    digitalWrite(INDICATOR, HIGH); // ESP8266 builtin LED is "backwards" i.e. active LOW
  else if (command == "DOWN")        // on
    digitalWrite(INDICATOR, LOW);  // ESP8266 builtin LED is "backwards" i.e. active LOW
}

//
// This function will be called when Alexa hears "switch [on | off] 'shade'"
//
void onVoiceCommand(bool onoff) {
  Serial.printf("onVoiceCommand %d\n", onoff);

  if (onoff)
    commandShade("DOWN");
  else
    commandShade("UP");
}

//
bool getAlexaState() {
  Serial.printf("getAlexaState %d\n", !digitalRead(INDICATOR));
  return !digitalRead(INDICATOR);
}

//
// The only constructor: const char* name, function<void(bool)> onCommand
// choose the name wisely: Alexa has very selective hearing!
//
weenyMo w("shade", onVoiceCommand);

void setup() {
  Serial.begin(115200);

  pinMode(INDICATOR, OUTPUT);
  EEPROM.begin(EEPROM_SIZE);

  Blinker.blink();

  WiFi.begin(WIFI_SSID, WIFI_PSK);
  WiFi.waitForConnectResult(); // so much neater than those stupid loops and dots
  w.gotIPAddress();            // ready to roll...Tell Alexa to discover devices.

  Blinker.blink();
  Blinker.blink();

  Serial.println("To program, long-press the program button on remote until blind goes up and down "
                 "slightly, then send 'PROGRAM' command via serial monitor.");
  Serial.println("Ready");
}

void loop() {
  // check if input is available
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    // Serial.println("got command\n"); Serial.print("."); Serial.print(command);
    // Serial.print(".\n");
    if (command == "UP" || command == "DOWN" || command == "MY" || command == "PROGRAM")
      commandShade(command);
  }

  Blinker.update();
}
