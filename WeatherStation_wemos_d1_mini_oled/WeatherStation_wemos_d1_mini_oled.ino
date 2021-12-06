// Main libs
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <RTClib.h>
#include <Array.h>
#include <MillisTimer.h>

#include "WeatherDisplay.h"
#include "DebugHelpers.h"

///////////////// DEFINES
#define OTA
#define WIFI_MANAGER

#ifdef DEBUG
#define CHECK_WEATHER_INTERVAL 1000 * 30
#define CHECK_SLEEP_TIME_INTERVAL 1000 * 20
#define CHECK_CONNECTION_TIME_INTERVAL 1000 * 5

#define CHECK_WEATHER_DECREASED_DUE_TO_FAIL_INTERVAL CHECK_WEATHER_INTERVAL
#else // DEBUG
#define CHECK_WEATHER_INTERVAL 1000 * 60 * 30
#define CHECK_SLEEP_TIME_INTERVAL 1000 * 60 * 10
#define CHECK_CONNECTION_TIME_INTERVAL 1000 * 60 * 1

#define CHECK_WEATHER_DECREASED_DUE_TO_FAIL_INTERVAL 1000 * 60 * 5
#endif // not DEBUG

#define DEVICE_NAME F("WeatherStation_OLED_1")

#ifdef WIFI_MANAGER
#define AP_WIFI_CONFIG_NAME "WPConfig"
#define STASSID WiFi.SSID()
#else // WIFI_MANAGER
#define STASSID String("ssid_name")
#define STAPSK "ssid_password"
#endif // not WIFI_MANAGER

#define WEATHER_REQUEST_URL F("http://api.openweathermap.org/data/2.5/onecall?lat=XX.XXXXXX&lon=XX.XXXXXX&units=metric&exclude=current,minutely,daily,alerts&appid=XXXXXXXXXXXXXXXXXXXXX")

#define DND_NIGHT_TIME 23
#define DND_MORNING_TIME 8

#define EVENING_TIME 18
#define MORNING_TIME 7

#define PROBABILITY_OF_PERCEPTION_MAX_COUNT 16

#define WEATHER_CONDITIONS_COUNT_MAX (int8_t)3


///////////////// GLOBALS
#if defined(OTA) || defined(WIFI_MANAGER)
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer webServer(80);
#endif // defined(OTA) || defined(WIFI_MANAGER)

#ifdef OTA
#include <AsyncElegantOTA.h>
#endif // OTA

#ifdef WIFI_MANAGER
#include "src/ESPConnect/ESPConnect.h"
#endif // WIFI_MANAGER

StaticJsonDocument<25000> jsonResponse;

CWeatherDisplay weatherDisplay;

MillisTimer connectionCheckTimer  = MillisTimer(CHECK_CONNECTION_TIME_INTERVAL);
MillisTimer weatherCheckTimer     = MillisTimer(CHECK_WEATHER_INTERVAL);
MillisTimer sleepTimeCheckTimer   = MillisTimer(CHECK_SLEEP_TIME_INTERVAL);

bool doNotDisturb = false;
bool lastRequestEndedWithError = false;

bool ntpFirstRun = true;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 1000 * 60 * 60 * 24); // update interval set to 24 hours

#ifdef TELEMETRY
#include "uptime_formatter.h"

String message = "";
bool messageReady = false;

#define UNITIALIZED_STR "Uninitialized"
struct SEspTelemetry
{
  String startedAt = UNITIALIZED_STR;
  String wiFiConnectedTo = UNITIALIZED_STR;
  IPAddress ipAdressObtained;
  unsigned long totalWeatherRequestsFromFirstStart = 0;
  unsigned long totalWeatherRequestsFailed = 0; 
} espTelemetry;
#endif // TELEMETRY

static const unsigned int weatherTypeWorstness[] PROGMEM = {
  202, 212, 232, 201, 200, 231, 230, 221, 211, 210,
  314, 302, 312, 313, 311, 321, 310, 301, 300,
  504, 503, 511, 502, 522, 501, 531, 521, 520, 500,
  622, 616, 621, 620, 615, 613, 612, 602, 611, 601, 600,
  701, 711, 721, 731, 741, 751, 761, 752, 771, 781,
  804, 803, 802, 801, 800
};

///////////////// FORWARD DECLARATIONS
#ifdef WIFI_MANAGER
void UpdateWiFiStatusAnimationCb();
void WiFiStatusFailCb();
#endif // WIFI_MANAGER

void CheckConnection(MillisTimer &mt);
void CheckWeather(MillisTimer &mt);
void CheckSleepTime(MillisTimer &mt);
unsigned int WorstWeatherCase(const Array<unsigned int, WEATHER_CONDITIONS_COUNT_MAX>& weatherArray);
#ifdef TELEMETRY
String GetTelemetry();
void MonitorSerialCommunication();
#endif // TELEMETRY

///////////////// CODE
void setup() {
  Serial.begin(9600);
  while (!Serial);

  DEBUG_LOG(F("Setup Begin Free heap: "));
  DEBUG_LOG_LN(ESP.getFreeHeap());

  weatherDisplay.Begin();
  
  // Start connection to WiFi network
  DEBUG_LOG_LN(F("Initialization strarted"));
  DEBUG_LOG_LN();
  DEBUG_LOG_LN();
  DEBUG_LOG(F("Connecting to "));
  DEBUG_LOG_LN(STASSID);

  WiFi.hostname(DEVICE_NAME);

  #ifdef WIFI_MANAGER
  ESPConnect.SetWiFiStatusUpdateCb(UpdateWiFiStatusAnimationCb);
  ESPConnect.SetWifiStatusFailCb(WiFiStatusFailCb);
  
  ESPConnect.autoConnect(AP_WIFI_CONFIG_NAME);

  if(ESPConnect.begin(&webServer))
  {
    DEBUG_LOG_LN("Failed to connect to WiFi");
    weatherDisplay.DisplayWiFiConfigurationHelpText(AP_WIFI_CONFIG_NAME);
  }
  else 
  {
    DEBUG_LOG_LN("Connected to WiFi");
    DEBUG_LOG_LN("IPAddress: "+WiFi.localIP().toString());    
    weatherDisplay.UpdateWiFiConnectedState(STASSID.c_str(), WiFi.localIP().toString());
  }  
  #else // WIFI_MANAGER
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);

  weatherDisplay.ResetAnimationFrames();
  while (WiFi.status() != WL_CONNECTED) {
    weatherDisplay.UpdateWiFiAnimation(STASSID.c_str());
    delay(500);
    DEBUG_LOG(F("."));
  }
  weatherDisplay.UpdateWiFiConnectedState(STASSID.c_str(), WiFi.localIP().toString());
  #endif // not WIFI_MANAGER
  
  weatherDisplay.EnableOLEDProtection(true);

// How long we'll display obtained IP adress
#ifdef DEBUG
  delay(1000);
#else // DEBUG
  delay(10000);
#endif // not DEBUG

#ifdef OTA
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, F("text/plain"), DEVICE_NAME);
  });

#ifdef TELEMETRY
  webServer.on("/telemetry", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, F("text/plain"), GetTelemetry().c_str());
  });
#endif // TELEMETRY

  AsyncElegantOTA.begin(&webServer);
  webServer.begin();
  DEBUG_LOG_LN(F("HTTP server started"));
#endif // OTA

  DEBUG_LOG_LN(F(""));
  DEBUG_LOG_LN(F("WiFi connected"));
  DEBUG_LOG_LN(F("IP address: "));
  DEBUG_LOG_LN(WiFi.localIP());
  DEBUG_LOG_LN(F(""));
  DEBUG_LOG_LN(F("Initialization end"));

#ifdef TELEMETRY
  espTelemetry.wiFiConnectedTo  = STASSID;
  espTelemetry.ipAdressObtained = WiFi.localIP();
#endif // TELEMETRY

  connectionCheckTimer.setInterval(CHECK_CONNECTION_TIME_INTERVAL);
  connectionCheckTimer.expiredHandler(CheckConnection);
  connectionCheckTimer.start();

  weatherCheckTimer.setInterval(CHECK_WEATHER_INTERVAL);
  weatherCheckTimer.expiredHandler(CheckWeather);
  weatherCheckTimer.start();

  sleepTimeCheckTimer.setInterval(CHECK_SLEEP_TIME_INTERVAL);
  sleepTimeCheckTimer.expiredHandler(CheckSleepTime);
  sleepTimeCheckTimer.start();

  timeClient.begin();
  timeClient.update();

  CheckWeather(weatherCheckTimer);
  CheckSleepTime(sleepTimeCheckTimer);

  DEBUG_LOG(F("Setup End Free heap: "));
  DEBUG_LOG_LN(ESP.getFreeHeap());
}

void loop() 
{
#ifdef OTA
  AsyncElegantOTA.loop();
#endif // OTA
  
  timeClient.update();

  connectionCheckTimer.run();
  weatherCheckTimer.run();
  sleepTimeCheckTimer.run();

  weatherDisplay.UpdateDisplay();

#ifdef TELEMETRY
  MonitorSerialCommunication();
#endif // TELEMETRY
}

#ifdef WIFI_MANAGER
void UpdateWiFiStatusAnimationCb()
{
  weatherDisplay.UpdateWiFiAnimation(STASSID.c_str());
}

void WiFiStatusFailCb()
{
  weatherDisplay.DisplayWiFiConfigurationHelpText(AP_WIFI_CONFIG_NAME);
}
#endif // WIFI_MANAGER

void CheckConnection(MillisTimer &mt)
{
  if ((WiFi.status() != WL_CONNECTED)) {
    DEBUG_LOG_LN(F("Reconnecting to WiFi..."));
    weatherDisplay.SetNoWifiConnectionMark(true);
    WiFi.reconnect();
  }
  else
  {
    weatherDisplay.SetNoWifiConnectionMark(false);
  }
}

void CheckWeather(MillisTimer &mt)
{
#ifdef TELEMETRY
  ++espTelemetry.totalWeatherRequestsFromFirstStart;
#endif // TELEMETRY

  DEBUG_LOG(F("Prepare request send Free heap: "));
  DEBUG_LOG_LN(ESP.getFreeHeap());
  
  DEBUG_LOG_LN(F("Sending request"));
  WiFiClient client;
  HTTPClient http;
  http.begin(client, WEATHER_REQUEST_URL);
  DEBUG_LOG_LN(WEATHER_REQUEST_URL);
  int httpResponseCode = http.GET();

  if (httpResponseCode == t_http_codes::HTTP_CODE_OK) 
  {
    DEBUG_LOG(F("Request done Free heap: "));
    DEBUG_LOG_LN(ESP.getFreeHeap());

    const char* payload = http.getString().c_str();

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(jsonResponse, payload);

    // Test if parsing succeeded.
    if (error) 
    {
      DEBUG_LOG(F("deserializeJson() failed: "));
      DEBUG_LOG_LN(error.f_str());
      DEBUG_LOG_LN(payload);

#ifdef TELEMETRY
      ++espTelemetry.totalWeatherRequestsFailed;
#endif // TELEMETRY

      weatherCheckTimer.setInterval(CHECK_WEATHER_DECREASED_DUE_TO_FAIL_INTERVAL);
      weatherCheckTimer.reset();
      weatherCheckTimer.start();
      
      lastRequestEndedWithError = true;
      weatherDisplay.SetErrorMark(true);
      
      return;
    }

    if(lastRequestEndedWithError)
    {
      weatherCheckTimer.setInterval(CHECK_WEATHER_INTERVAL);
      weatherCheckTimer.reset();
      weatherCheckTimer.start();
      
      lastRequestEndedWithError = false;
      weatherDisplay.SetErrorMark(false);
    }
    
    SWeatherInfo weatherInfo;

    Array<unsigned int, WEATHER_CONDITIONS_COUNT_MAX> weatherConditions;

    int dt = jsonResponse["list"][0]["dt"];
    DEBUG_LOG(F("[dt="));
    DEBUG_LOG(dt);
    DEBUG_LOG_LN(F("]"));

    // Prophet rain for icon
    JsonArray arr = jsonResponse["hourly"];
    for(JsonObject repo : arr)
    {
      const int weatherCode = repo["weather"][0]["id"];
      
      DEBUG_LOG(F("[weatherCode="));
      DEBUG_LOG(weatherCode);
      DEBUG_LOG(F("] "));

      weatherConditions.push_back(weatherCode);
      if(weatherConditions.size() == WEATHER_CONDITIONS_COUNT_MAX)
      {
        break;
      }
    }

    // Gather probability of perception
    Array<float, PROBABILITY_OF_PERCEPTION_MAX_COUNT> perceptionAll;

    const int timezoneOffset = jsonResponse["timezone_offset"];

    timeClient.setTimeOffset(timezoneOffset);

    DEBUG_LOG_LN(F(""));
    DEBUG_LOG(F("POP: "));
    arr = jsonResponse["hourly"];
    for(JsonObject repo : arr)
    {
      const float perception = repo["pop"];
      perceptionAll.push_back(perception);

      DEBUG_LOG(perception);
      DEBUG_LOG(", ");

      if(perceptionAll.size() == PROBABILITY_OF_PERCEPTION_MAX_COUNT)
      {
        break;
      }
    }

    // Get temp
    const float currentTemperatureRaw = jsonResponse["hourly"][0]["feels_like"];

    double intpart;
    const short currentTemperature = modf(currentTemperatureRaw, &intpart) >= 0.5f ? ceil(currentTemperatureRaw) : floor(currentTemperatureRaw);

    DEBUG_LOG_LN(F(""));
    DEBUG_LOG(F("Current temperature: "));
    DEBUG_LOG_LN(currentTemperature);

    float midnightTemperatureRaw = 0;
    arr = jsonResponse["hourly"];
    bool skippedFirst = false; 
    for(JsonObject repo : arr)
    {
      if(!skippedFirst)
      {
        skippedFirst = true;
        continue;
      }
      const unsigned long epochTime = repo["dt"];
      DateTime weatherTime(epochTime + timezoneOffset);
      if(weatherTime.hour() == 0)
      {
        midnightTemperatureRaw = repo["feels_like"];
        break;
      }
    }

    const short midnightTemperature = modf(midnightTemperatureRaw, &intpart) >= 0.5f ? ceil(midnightTemperatureRaw) : floor(midnightTemperatureRaw);

    DEBUG_LOG_LN(F(""));
    DEBUG_LOG(F("Midnight temperature: "));
    DEBUG_LOG_LN(midnightTemperature);

    weatherInfo.m_weatherId   = WorstWeatherCase(weatherConditions);
    weatherInfo.m_pop         = perceptionAll;
    weatherInfo.m_currentTemp = currentTemperature;
    weatherInfo.m_eveningTemp = midnightTemperature;

    DEBUG_LOG_LN(F(""));
    DEBUG_LOG(F("Worst weather: "));
    DEBUG_LOG_LN(weatherInfo.m_weatherId);

    weatherDisplay.SetWeatherInfo(weatherInfo);

    bool isDay = timeClient.getHours() >= EVENING_TIME || timeClient.getHours() <= MORNING_TIME ? false : true;
    weatherDisplay.SetIsDay(isDay);
  }
  else {
    DEBUG_LOG(F("Error code: "));
    DEBUG_LOG(httpResponseCode);
    DEBUG_LOG(F(" "));
    DEBUG_LOG_LN(http.errorToString(httpResponseCode));

    lastRequestEndedWithError = true;
    weatherDisplay.SetErrorMark(true);    
  }
  
  // Free resources
  http.end();
}

void CheckSleepTime(MillisTimer &mt)
{
  DEBUG_LOG(F("[NTP] Current time: "));
  DEBUG_LOG(timeClient.getFormattedTime());
  DEBUG_LOG_LN(F(";"));

  if(timeClient.getHours() == DND_NIGHT_TIME || timeClient.getHours() <= DND_MORNING_TIME)
  {
    doNotDisturb = true;
  }
  else
  {
    doNotDisturb = false;
  }

  if(!ntpFirstRun)
  {
    weatherDisplay.SetDoNotDisturb(doNotDisturb);
    
    bool isDay = timeClient.getHours() >= EVENING_TIME || timeClient.getHours() <= MORNING_TIME ? false : true;
    weatherDisplay.SetIsDay(isDay);
  }
  else
  {
    ntpFirstRun = false;
  }
}

unsigned int WorstWeatherCase(const Array<unsigned int, WEATHER_CONDITIONS_COUNT_MAX>& weatherArray)
{
  unsigned int worstCase = 0;
  uint8_t compareIndex = 0xFF;

  for(unsigned int weatherCondition : weatherArray)
  {
    for(uint8_t index = 0; index < sizeof(weatherTypeWorstness) / sizeof(int) && index < compareIndex; ++index)
    {
      if(pgm_read_word_near(&weatherTypeWorstness[index]) == weatherCondition)
      {
        worstCase = weatherCondition;
        compareIndex = index;
      }
    }
  }

  return worstCase;
}

#ifdef TELEMETRY
String GetTelemetry()
{
    String result = F("Telemetry\n============================\nUpTime: ");
    result += uptime_formatter::getUptime();
    
    result += F("\nwiFiConnectedTo: ");
    result += espTelemetry.wiFiConnectedTo;

    result += F("\nipAdressObtained: ");
    result += espTelemetry.ipAdressObtained.toString();

    result += F("\ntotalWeatherRequestsFromFirstStart: ");
    result += espTelemetry.totalWeatherRequestsFromFirstStart;

    result += F("\ntotalWeatherRequestsFailed: ");
    result += espTelemetry.totalWeatherRequestsFailed;

    result += F("\ndoNotDisturb: ");
    result += doNotDisturb ? F("True") : F("False");

    result += F("\n============================");

    return result;
}

void MonitorSerialCommunication()
{
#ifdef DEBUG
  while(Serial.available()) {
    message = Serial.readString();
    messageReady = true;
  }
  // Only process message if there's one
  if(messageReady) {
    // The only messages we'll parse will be formatted in JSON
    DynamicJsonDocument doc(1024); // ArduinoJson version 6+
    // Attempt to deserialize the message
    DeserializationError error = deserializeJson(doc,message);
    if(error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      messageReady = false;
      return;
    }
    if(doc["type"] == "telemetry_esp")
    {
      Serial.println(GetTelemetry());
    }
    messageReady = false;
  }
#endif // DEBUG
}
#endif // TELEMETRY
