#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncWebServer.h>

#include "Somfy_Remote.h"
#include "credentials.h"
#include "weenyMo.h"

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

// Create a separate web server for REST API (port 8080 to avoid conflict with weenyMo)
static AsyncWebServer restServer (8080);

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

// REST API handlers
static void handleShadeControl (AsyncWebServerRequest * request) {
    String action = "";
    String source = "REST";

    // Check for action parameter
    if (request->hasParam ("action")) {
        action = request->getParam ("action")->value();
        action.toUpperCase();
    }

    // Check for source parameter (optional)
    if (request->hasParam ("source")) {
        source = request->getParam ("source")->value();
    }

    Serial.printf ("REST API called - Action: %s, Source: %s\n", action.c_str(), source.c_str());

    // Process the action
    if (action == "ON" || action == "DOWN") {
        commandShade ("DOWN");
        request->send (200, "application/json", "{\"status\":\"success\",\"action\":\"down\",\"message\":\"Shade moving down\"}");
    }
    else if (action == "OFF" || action == "UP") {
        commandShade ("UP");
        request->send (200, "application/json", "{\"status\":\"success\",\"action\":\"up\",\"message\":\"Shade moving up\"}");
    }
    else if (action == "STOP" || action == "MY") {
        commandShade ("MY");
        request->send (200, "application/json", "{\"status\":\"success\",\"action\":\"stop\",\"message\":\"Shade stopped\"}");
    }
    else if (action == "PROGRAM") {
        commandShade ("PROGRAM");
        request->send (200, "application/json", "{\"status\":\"success\",\"action\":\"program\",\"message\":\"Programming mode activated\"}");
    }
    else {
        request->send (400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid action. Use: on/off/up/down/stop/program\"}");
    }
}

static void handleStatus (AsyncWebServerRequest * request) {
    bool          currentState = getAlexaState();
    String        stateStr     = currentState ? "down" : "up";
    unsigned long uptime       = millis() / 1000;

    String response = "{";
    response += "\"status\":\"success\",";
    response += "\"shade_position\":\"" + stateStr + "\",";
    response += "\"uptime_seconds\":" + String (uptime) + ",";
    response += "\"wifi_connected\":" + String (WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    response += "\"ip_address\":\"" + WiFi.localIP().toString() + "\",";
    response += "\"hostname\":\"shade.local\",";  // Add hostname info
    response += "\"free_heap\":" + String (ESP.getFreeHeap());
    response += "}";

    request->send (200, "application/json", response);
}

static void handleReboot (AsyncWebServerRequest * request) {
    request->send (200, "application/json", "{\"status\":\"success\",\"message\":\"Device will reboot in 3 seconds\"}");
    delay (3000);
    ESP.restart();
}

static void handleHelp (AsyncWebServerRequest * request) {
    String help = "<!DOCTYPE html><html><head><title>Shade Controller API</title></head><body>";
    help += "<h1>Shade Controller REST API</h1>";
    help += "<h2>Device Access:</h2>";
    help += "<ul>";
    help += "<li><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</li>";
    help += "<li><strong>Hostname:</strong> shade.local</li>";
    help += "<li><strong>Alexa Port:</strong> 80 (weenyMo)</li>";
    help += "<li><strong>REST API Port:</strong> 8080</li>";
    help += "</ul>";
    help += "<h2>Control Endpoints:</h2>";
    help += "<ul>";
    help += "<li><strong>GET /shade?action=on</strong> - Move shade down</li>";
    help += "<li><strong>GET /shade?action=off</strong> - Move shade up</li>";
    help += "<li><strong>GET /shade?action=up</strong> - Move shade up</li>";
    help += "<li><strong>GET /shade?action=down</strong> - Move shade down</li>";
    help += "<li><strong>GET /shade?action=stop</strong> - Stop shade movement</li>";
    help += "<li><strong>GET /shade?action=program</strong> - Activate programming mode</li>";
    help += "</ul>";
    help += "<h2>Status Endpoints:</h2>";
    help += "<ul>";
    help += "<li><strong>GET /status</strong> - Get device status (JSON)</li>";
    help += "<li><strong>GET /reboot</strong> - Reboot device</li>";
    help += "<li><strong>GET /help</strong> - This help page</li>";
    help += "</ul>";
    help += "<h2>Examples (using hostname):</h2>";
    help += "<pre>";
    help += "curl http://shade.local:8080/shade?action=down\n";
    help += "curl http://shade.local:8080/status\n";
    help += "</pre>";
    help += "<h2>Examples (using IP):</h2>";
    help += "<pre>";
    help += "curl http://" + WiFi.localIP().toString() + ":8080/shade?action=down\n";
    help += "curl http://" + WiFi.localIP().toString() + ":8080/status\n";
    help += "</pre>";
    help += "</body></html>";

    request->send (200, "text/html", help);
}

static void setupRestAPI () {
    // CORS headers for all responses
    DefaultHeaders::Instance().addHeader ("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader ("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    DefaultHeaders::Instance().addHeader ("Access-Control-Allow-Headers", "Content-Type");

    // Main shade control endpoint
    restServer.on ("/shade", HTTP_GET, handleShadeControl);

    // Status endpoint
    restServer.on ("/status", HTTP_GET, handleStatus);

    // Reboot endpoint
    restServer.on ("/reboot", HTTP_GET, handleReboot);

    // Help/documentation endpoint
    restServer.on ("/help", HTTP_GET, handleHelp);
    restServer.on ("/", HTTP_GET, handleHelp);

    // Handle 404 errors
    restServer.onNotFound ([] (AsyncWebServerRequest * request) {
        request->send (404, "application/json", "{\"status\":\"error\",\"message\":\"Endpoint not found. Visit /help for API documentation\"}");
    });

    // Start the REST API server
    restServer.begin();
    Serial.printf ("REST API server started on port 8080\n");
    Serial.printf ("API documentation available at: http://%s:8080/help\n", WiFi.localIP().toString().c_str());
    Serial.printf ("API also available via hostname: http://shade.local:8080/help\n");
}

static void setupMDNS () {
    // Start mDNS service
    if (MDNS.begin ("shade")) {
        Serial.println ("mDNS responder started successfully");
        Serial.println ("Device accessible as 'shade.local'");

        // Add service advertisements - the ESP8266mDNS library will handle both ports
        MDNS.addService ("http", "tcp", 80);         // Alexa/weenyMo service
        MDNS.addService ("shade-api", "tcp", 8080);  // REST API service (use custom service name)

        // Add TXT records for service discovery (only 4 parameters: service, protocol, key, value)
        MDNS.addServiceTxt ("http", "tcp", "device", "shade-controller");
        MDNS.addServiceTxt ("http", "tcp", "alexa", "enabled");
        MDNS.addServiceTxt ("shade-api", "tcp", "api", "rest");
        MDNS.addServiceTxt ("shade-api", "tcp", "description", "Shade Control REST API");
        MDNS.addServiceTxt ("shade-api", "tcp", "version", "1.0");

        Serial.println ("mDNS services advertised:");
        Serial.println ("  - http._tcp.local:80 (Alexa/weenyMo service)");
        Serial.println ("  - shade-api._tcp.local:8080 (REST API)");
    }
    else {
        Serial.println ("ERROR: Failed to start mDNS responder");
    }
}

static weenyMo weeny ("shade", onVoiceCommand);  // choose the name wisely: Alexa has very selective hearing!

static unsigned long       boot_time;
static const unsigned long DAILY_REBOOT_INTERVAL = 24UL * 60UL * 60UL * 1000UL;  // 24 hours in milliseconds

void setup () {
    boot_time = millis();
    Serial.begin (9600);

    pinMode (INDICATOR, OUTPUT);
    digitalWrite (INDICATOR, HIGH);  // ESP8266 builtin LED is "backwards" i.e. active LOW

    blinker.blink();

    // Set hostname before connecting to WiFi
    WiFi.hostname ("shade");

    WiFi.begin (WIFI_SSID, WIFI_PSK);  // defined in credentials.h
    WiFi.waitForConnectResult();       // so much neater than those stupid loops and dots

    // Setup mDNS after WiFi connection
    setupMDNS();

    // Initialize SomfyRemote after EEPROM is ready
    somfy = new SomfyRemote ("somfy", 0x781413);
    delay (100);  // Give it time to initialize

    weeny.gotIPAddress();  // ready to roll...Tell Alexa to discover devices.

    // Setup REST API
    setupRestAPI();

    blinker.blink();
    blinker.blink();

    Serial.println ("To program, long-press the program button on remote until blind goes up and down "
                    "slightly, then send 'PROGRAM' command via serial monitor or REST API.");
    Serial.printf ("REST API available at: http://%s:8080/help\n", WiFi.localIP().toString().c_str());
    Serial.println ("REST API also available at: http://shade.local:8080/help");
    Serial.println ("Alexa service available at: http://shade.local");
    Serial.println ("Ready");
}

void loop () {
    MDNS.update();

    if ((millis() - boot_time) > DAILY_REBOOT_INTERVAL) {
        Serial.println ("Daily reboot timer triggered - restarting device");
        ESP.restart();
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println ("WiFi disconnected - restarting device");
        ESP.restart();
    }

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
