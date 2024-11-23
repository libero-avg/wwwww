#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include "esp_wifi.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>

gpio_num_t LED = GPIO_NUM_2;
uint8_t ledState = 0;

const char* ssid = "ESP32_Access_Point";
const char* password = "12345678";

WebServer server(80);
DNSServer dnsServer;

String formatMacAddress(uint8_t* mac) {
    String macAddr = "";
    for (int i = 0; i < 6; i++) {
        if (mac[i] < 0x10) macAddr += "0";
        macAddr += String(mac[i], HEX);
        if (i < 5) macAddr += ":";
    }
    macAddr.toUpperCase();
    return macAddr;
}

void handleRoot() {
    String html = "<html><head><style>"
                  "body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #121212; color: #ffffff; }"
                  "h1 { color: #39ff14; text-align: center; padding: 20px 0; margin: 0; }"
                  ".container-wrapper { display: flex; justify-content: center; margin-top: 20px; }"
                  ".container { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 15px; padding: 20px; max-width: 600px; }"
                  ".card { background-color: #1e1e1e; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 255, 128, 0.3); padding: 15px; }"
                  "ul { list-style-type: none; padding: 0; margin: 0; }"
                  "li { margin: 5px 0; color: #a6fffd; font-size: 0.9em; }"
                  "footer { margin-top: 20px; font-size: 0.8em; color: #555555; text-align: center; padding: 10px 0; background-color: #0f0f0f; }"
                  "</style><meta charset='utf-8' /></head><body>"
                  "<h1>Pobliskie sieci Wi-Fi i podłączone urządzenia</h1>"
                  "<div class='container-wrapper'><div class='container'>";

    html += "<div class='card'><h2>Pobliskie sieci Wi-Fi</h2><ul>";
    int n = WiFi.scanNetworks();
    std::vector<String> uniqueSSIDs;

    if (n == 0) {
        html += "<li>Brak sieci Wi-Fi w zasięgu.</li>";
    } else {
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);

            if (std::find(uniqueSSIDs.begin(), uniqueSSIDs.end(), ssid) == uniqueSSIDs.end()) {
                uniqueSSIDs.push_back(ssid);
                html += "<li>";
                html += ssid;
                html += " (RSSI: ";              
                html += WiFi.RSSI(i);
                html += " dBm)";
                html += "</li>";
            }
        }
    }
    html += "</ul></div>";

    html += "<div class='card'><h2>Podłączone urządzenia</h2><ul>";
    uint8_t numStations = WiFi.softAPgetStationNum();
    if (numStations == 0) {
        html += "<li>Brak podłączonych urządzeń.</li>";
    } else {
        wifi_sta_list_t stationList;
        memset(&stationList, 0, sizeof(stationList));

        esp_err_t result = esp_wifi_ap_get_sta_list(&stationList);

        if (result == ESP_OK) {
            for (uint8_t i = 0; i < stationList.num; i++) {
                html += "<li>Adres MAC: ";
                html += formatMacAddress(stationList.sta[i].mac); 
                html += "</li>";
            }
        } else {
            html += "<li>Nie można pobrać listy stacji.</li>";
        }
    }

    html += "</ul></div>";
    html += "<div class='card'><h2>Sterowanie LED</h2><button onclick=\"toggleLED()\">Włącz/Wyłącz LED</button></div>";
    html += "</div></div><footer>Wszystkie prawa zastrzeżone © 2024 @lolowiczz</footer>";
    html += "<script>function toggleLED() { fetch('/toggleLED').then(response => response.text()).then(data => console.log(data)); }</script>";
    html += "</body></html>";

    server.send(200, "text/html", html); 
}

void handleToggleLED() {
    static bool ledState = false;
    ledState = !ledState;
    digitalWrite(LED, ledState ? HIGH : LOW);
    server.send(200, "text/plain", ledState ? "LED is ON" : "LED is OFF");
}

void handleAbout() {
    Serial.println("Obsługa żądania about");

    if (!SPIFFS.begin(true)) {
        Serial.println("Nie udało się zamontować SPIFFS");
        server.send(500, "text/plain", "Internal Server Error");
        return;
    }

    File file = SPIFFS.open("/about.html", "r");
    if (!file) {
        Serial.println("Nie udało się otworzyć about.html");
        server.send(404, "text/plain", "Plik nie znaleziony");
        return;
    }

    String html = file.readString();
    Serial.println("Wysyłanie zawartości about.html:");
    Serial.println(html);
    server.send(200, "text/html", html);
    file.close();
}

void handleNotFound() {
    Serial.println("Obsługa żądania nie znaleziono");
    server.send(404, "text/plain", "404: Nie znaleziono");
}

void setup() {
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);

    Serial.begin(115200);
    Serial.println("Rozpoczynanie konfiguracji...");

    // Konfiguracja ESP32 jako Access Point
    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("Adres IP Access Point: ");
    Serial.println(IP);

    // Konfiguracja serwera DNS
    dnsServer.start(53, "*", IP);

    if (!MDNS.begin("esp32")) {
        Serial.println("Error setting up MDNS responder!");
        while (1) {
            delay(1000);
        }
    }
    Serial.println("mDNS responder started");

    server.on("/", handleRoot);
    server.on("/toggleLED", handleToggleLED);
    server.on("/about", handleAbout);
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("Serwer HTTP uruchomiony. Wejdź na stronę http://esp32.local");
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
}