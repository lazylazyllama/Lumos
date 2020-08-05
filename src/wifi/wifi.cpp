#include "wifi.hpp"

static AsyncWebServer webServer(80);
static DNSServer dnsServer;
static AsyncWiFiManager wifiManager(&webServer,&dnsServer);

AsyncWiFiManagerParameter customDeviceNameParameter("");

void Incendio::Wifi::begin(void) {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Incendio::Config::load();

  customDeviceNameParameter = AsyncWiFiManagerParameter("device name", "Custom device name e.g. Kitchen Light", Incendio::Config::deviceName.c_str(), 40);
  
  wifiManager.addParameter(&customDeviceNameParameter);
  wifiManager.setCustomHeadElement(HTTP_CUSTOM_STYLE);
  wifiManager.setSaveConfigCallback(saveCustomParameters);

  wifiManager.autoConnect("Webthings Setup", "password");
  while (WiFi.status() != WL_CONNECTED);
  
  String hostName = "Webthings Device";
  if (!Incendio::Config::deviceName.equals("")) {
    hostName.concat(": ");
    hostName.concat(Incendio::Config::deviceName);
  }  
  
  #ifdef ESP32
    WiFi.setHostname(hostName.c_str());
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    WiFi.setSleep(true); //this would use WIFI_PS_MIN_MODEM
  #elif defined(ESP8266)
    WiFi.hostname(hostName.c_str());
    WiFi.setSleepMode(WIFI_MODEM_SLEEP);
    WiFi.forceSleepBegin();
  #endif
  
  digitalWrite(LED_BUILTIN, LOW);
}

void Incendio::Wifi::saveCustomParameters(void) {
  Incendio::Config::deviceName = customDeviceNameParameter.getValue();
  Incendio::Config::save();
}