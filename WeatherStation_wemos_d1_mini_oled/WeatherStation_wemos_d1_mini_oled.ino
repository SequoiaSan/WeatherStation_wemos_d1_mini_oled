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

#include <FS.h>
#include "ConfigHTMLPage.h"

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

#define DEVICE_NAME "WeatherStation_OLED_1"

#ifdef WIFI_MANAGER
#define AP_WIFI_CONFIG_NAME "WPConfig"
#define STASSID WiFi.SSID()
#else // WIFI_MANAGER
#define STASSID String("ssid_name")
#define STAPSK "ssid_password"
#endif // not WIFI_MANAGER

const char* weatherRequestURL = "http://api.openweathermap.org/data/2.5/onecall?lat=%f&lon=%f&units=%s&exclude=current,minutely,daily,alerts&appid=%s";

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

StaticJsonDocument<22000> jsonResponse;
StaticJsonDocument<400> deviceConfiguration;
bool configurationUpdated = false;

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

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", F("Not found"));
}

void ApplyConfigurataion()
{
    // Will apply after restart. Do you wish to restart?
    WiFi.hostname(deviceConfiguration[0][PARAM_WIFINAME].as<String>());
    weatherDisplay.EnableOLEDProtection(deviceConfiguration[0][PARAM_SCREENSAVER].as<bool>(), deviceConfiguration[0][PARAM_SCREENSAVERTIME].as<int>(), deviceConfiguration[0][PARAM_SCREENSAVERTIMEOFF].as<int>());
    CheckSleepTime(sleepTimeCheckTimer);
    weatherDisplay.SetCelsiusSign(deviceConfiguration[0][PARAM_CELSIUSSIGN].as<bool>() && deviceConfiguration[0][PARAM_CELSIUS].as<bool>());
    weatherDisplay.SetDisplayRotation(deviceConfiguration[0][PARAM_ROTATEDISPLAY].as<bool>());
    CheckWeather(weatherCheckTimer); 
}

void ReadConfigurationFile()
{
  DEBUG_LOG_LN(F("ReadConfigurationFile()"));
  if(!configurationUpdated)
  {
    configurationUpdated = false;
    DEBUG_LOG_LN(F("Open configuration file"));
    File configuration = SPIFFS.open("/configuration.json", "r");
    if(configuration && configuration.size()) 
    {
      JsonArray jsonArray = deviceConfiguration.to<JsonArray>();
      DeserializationError err = deserializeJson(deviceConfiguration, configuration);
      DEBUG_LOG_LN(configuration);
      if (err) {
        DEBUG_LOG(F("deserializeJson() failed with code "));
        DEBUG_LOG_LN(err.c_str());
      }

      DEBUG_LOG_LN("Read configuration file successfully.");
      configuration.close();
    }
    else
    {
      DEBUG_LOG_LN("Failed to read file.");
    }
  }  
}

// Replaces placeholder with stored values
String processor(const String& var){
  ReadConfigurationFile();

  if(var == PARAM_WIFINAME)
  {
    return deviceConfiguration[0][PARAM_WIFINAME].as<String>();
  }
  else if (var == PARAM_COORDINATES)
  {
    return String(deviceConfiguration[0][PARAM_LAT].as<String>() + ", " + deviceConfiguration[0][PARAM_LON].as<String>());
  }
  else if (var == PARAM_LAT)
  {
    return deviceConfiguration[0][PARAM_LAT].as<String>();
  }
  else if (var == PARAM_LON)
  {
    return deviceConfiguration[0][PARAM_LON].as<String>();
  }
  else if (var == PARAM_SCREENSAVER)
  {
      if(deviceConfiguration[0][PARAM_SCREENSAVER].as<bool>())
      {
        return String(F("checked"));
      }
      else
      {
        return String();
      }
  }
  else if (var == PARAM_SCREENSAVERTIME)
  {
    return String(static_cast<int>(deviceConfiguration[0][PARAM_SCREENSAVERTIME].as<int>()/1000/60));
  }
  else if (var == PARAM_SCREENSAVERTIMEOFF)
  {
    return String(static_cast<int>(deviceConfiguration[0][PARAM_SCREENSAVERTIMEOFF].as<int>()/1000));
  }
  else if (var == PARAM_DNDMODE)
  {
      if(deviceConfiguration[0][PARAM_DNDMODE].as<bool>())
      {
        return String(F("checked"));
      }
      else
      {
        return String();
      }
  }
  else if (var == PARAM_DNDFROM)
  {
    return deviceConfiguration[0][PARAM_DNDFROM].as<String>();
  }
  else if (var == PARAM_DNDTO)
  {
    return deviceConfiguration[0][PARAM_DNDTO].as<String>();
  }
  else if (var == PARAM_CELSIUS)
  {
      if(deviceConfiguration[0][PARAM_CELSIUS].as<bool>())
      {
        return String(F("checked"));
      }
      else
      {
        return String();
      }
  }
  else if (var == PARAM_CELSIUSSIGN)
  {
      if(deviceConfiguration[0][PARAM_CELSIUSSIGN].as<bool>())
      {
        return String(F("checked"));
      }
      else
      {
        return String();
      }
  }
  else if (var == PARAM_ROTATEDISPLAY)
  {
      if(deviceConfiguration[0][PARAM_ROTATEDISPLAY].as<bool>())
      {
        return String(F("checked"));
      }
      else
      {
        return String();
      }
  } 
  else if (var == PARAM_APIKEY)
  {
    return deviceConfiguration[0][PARAM_APIKEY].as<String>();
  }
#ifdef TELEMETRY
  else if (var == PARAM_TELEMETRY)
  {
    return String("<a style=\"text-decoration: none;\" href=\"/telemetry\"><button class=\"button buttonSmall\" style=\"margin: auto; display: flex;\" onclick=\"\">Telemetry</button></a>");
  }
#endif // TELEMETRY

  return String();
}

void setup() 
{
  Serial.begin(9600);
  while (!Serial);

  DEBUG_LOG(F("Setup Begin Free heap: "));
  DEBUG_LOG_LN(ESP.getFreeHeap());

  SPIFFS.begin();
  if(!SPIFFS.exists(F("/configuration.json")))
  {
    File configuration = SPIFFS.open(F("/configuration.json"), "w");

    String jsonData;
    JsonObject obj = deviceConfiguration.createNestedObject();
    obj[PARAM_WIFINAME] = DEVICE_NAME;
    obj[PARAM_LAT] = 0;
    obj[PARAM_LON] = 0;
    obj[PARAM_SCREENSAVER] = true;
    obj[PARAM_SCREENSAVERTIME] = WEATHER_DISPLAY_OLED_START_REFRESH * 1000 * 60;
    obj[PARAM_SCREENSAVERTIMEOFF] = WEATHER_DISPLAY_OLED_END_REFRESH * 1000;
    obj[PARAM_DNDMODE] = true;
    obj[PARAM_DNDFROM] = 23;
    obj[PARAM_DNDTO] = 7;
    obj[PARAM_CELSIUS] = true;
    obj[PARAM_CELSIUSSIGN] = true;
    obj[PARAM_ROTATEDISPLAY] = false;
    obj[PARAM_APIKEY] = "XXXXXXXXXXXXXXXXXXXXXXXXXX";
    serializeJson(deviceConfiguration, jsonData);
  
    DEBUG_LOG_LN(jsonData.c_str());
    
    const int bytesWritten = configuration.print(jsonData.c_str());
    if (bytesWritten > 0) 
    {
      configurationUpdated = true;
      DEBUG_LOG(F("File was written "));
      DEBUG_LOG_LN(bytesWritten);
     
    } else 
    {
        DEBUG_LOG_LN(F("File write failed"));
    }
    
    configuration.close();
  }
  else
  {
    ReadConfigurationFile();
  }

  weatherDisplay.Begin();
  
  // Start connection to WiFi network
  DEBUG_LOG_LN(F("Initialization strarted"));
  DEBUG_LOG_LN();
  DEBUG_LOG_LN();
  DEBUG_LOG(F("Connecting to "));
  DEBUG_LOG_LN(STASSID);

  WiFi.hostname(deviceConfiguration[0][PARAM_WIFINAME].as<String>());

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
  
  weatherDisplay.EnableOLEDProtection(deviceConfiguration[0][PARAM_SCREENSAVER].as<bool>(), deviceConfiguration[0][PARAM_SCREENSAVERTIME].as<int>(), deviceConfiguration[0][PARAM_SCREENSAVERTIMEOFF].as<int>());
  weatherDisplay.SetCelsiusSign(deviceConfiguration[0][PARAM_CELSIUSSIGN].as<bool>() && deviceConfiguration[0][PARAM_CELSIUS].as<bool>());

  weatherDisplay.SetDisplayRotation(deviceConfiguration[0][PARAM_ROTATEDISPLAY].as<bool>());

// How long we'll display obtained IP adress
#ifdef DEBUG
  delay(1000);
#else // DEBUG
  delay(10000);
#endif // not DEBUG



#ifdef OTA
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", config_html_page, processor);
  });

  webServer.on("/restartdevice", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", "Restarting...");
    ESP.restart();
    // Reload page after 15 seconds? Progress Bar?
  });

  webServer.on("/resetdevice", HTTP_GET, [](AsyncWebServerRequest *request){
    String operationResult = F("Failed. Configuration wasn't wiped out. Restarting.");
    if(SPIFFS.remove(F("/configuration.json")))
    {
      operationResult = F("Success. Device cleared. Restarting.");
    }
    request->send_P(200, "text/html", operationResult.c_str());
    //ESPConnect.erase();
    WiFi.disconnect(true);
    ESP.eraseConfig();
    ESP.restart();
    // Progress Bar?
  });

#ifdef TELEMETRY
  webServer.on("/telemetry", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, F("text/plain"), GetTelemetry().c_str());
  });
#endif // TELEMETRY

  webServer.on("/saveconfig", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String operationResult;
    if(SPIFFS.exists(F("/configuration.json")))
    {
      File configuration = SPIFFS.open(F("/configuration.json"), "w");

      //
      // CHECK DATA BEFORE SAVE!!!!
      //
      // Check received coordinates
      float newLat = deviceConfiguration[0][PARAM_LAT].as<float>();
      float newLon = deviceConfiguration[0][PARAM_LON].as<float>();
      
      DEBUG_LOG(F("Previous Coordinates: "));
      DEBUG_LOG(newLat);
      DEBUG_LOG(F(", "));
      DEBUG_LOG_LN(newLon);
      
      if(request->hasParam(PARAM_COORDINATES))
      {
        const String paramValue = request->getParam(PARAM_COORDINATES)->value();
        
        DEBUG_LOG(F("Received new coordinates: "));
        DEBUG_LOG_LN(paramValue);
        
        float latTemp = 0;
        float lonTemp = 0;
        const int delimiterPos = paramValue.indexOf(",");
        
        DEBUG_LOG(F("Delimiter position: "));
        DEBUG_LOG_LN(delimiterPos);
        
        if(delimiterPos >= 0)
        {

          String latStr = paramValue.substring(0, delimiterPos);
          latStr.trim();
          latTemp = latStr.toFloat();

          String lonStr = paramValue.substring(delimiterPos + 1, paramValue.length());
          lonStr.trim();
          lonTemp = lonStr.toFloat();

          DEBUG_LOG(F("Transformed Coordinates: "));
          DEBUG_LOG(latTemp);
          DEBUG_LOG(F(", "));
          DEBUG_LOG_LN(lonTemp);

          if(latTemp > 0.f && lonTemp > 0.f)
          {
            newLat = latTemp;
            newLon = lonTemp;
          }
          
          DEBUG_LOG(F("New Coordinates: "));
          DEBUG_LOG(newLat);
          DEBUG_LOG(F(", "));
          DEBUG_LOG_LN(newLon);
        }
      }

      // Check Display off timer
      int newScreenSaverTime = deviceConfiguration[0][PARAM_SCREENSAVERTIME].as<int>();
      if(request->hasParam(PARAM_SCREENSAVERTIME) && request->getParam(PARAM_SCREENSAVERTIME)->value().toInt() > 0)
      {
        newScreenSaverTime = request->getParam(PARAM_SCREENSAVERTIME)->value().toInt();
      }

      int newScreenSaverTimeOff = deviceConfiguration[0][PARAM_SCREENSAVERTIMEOFF].as<int>();
      if(request->hasParam(PARAM_SCREENSAVERTIME) && request->getParam(PARAM_SCREENSAVERTIMEOFF)->value().toInt() > 0)
      {
        newScreenSaverTimeOff = request->getParam(PARAM_SCREENSAVERTIMEOFF)->value().toInt();
      }

      // Check DND time
      int newDNDTimeFrom = deviceConfiguration[0][PARAM_DNDFROM].as<int>();
      int newDNDTimeTo = deviceConfiguration[0][PARAM_DNDTO].as<int>();
       
      if(request->hasParam(PARAM_DNDFROM))
      {
        const int receivedDNDTimeFrom = request->getParam(PARAM_DNDFROM)->value().toInt();
        if(receivedDNDTimeFrom > 0 && receivedDNDTimeFrom <= 24)
        {
          newDNDTimeFrom = receivedDNDTimeFrom;
        }
        else if(newDNDTimeTo == newDNDTimeFrom)
        {
          newDNDTimeFrom = --newDNDTimeFrom < 1 ? 24 : newDNDTimeFrom;
        }
      }

      if(request->hasParam(PARAM_DNDTO))
      {
        const int receivedDNDTimeTo = request->getParam(PARAM_DNDTO)->value().toInt();
        if(receivedDNDTimeTo > 0 && receivedDNDTimeTo <= 24 && newDNDTimeFrom != receivedDNDTimeTo)
        {
          newDNDTimeTo = receivedDNDTimeTo;
        }
        else if(newDNDTimeTo == newDNDTimeFrom)
        {
          newDNDTimeTo = ++newDNDTimeTo > 24 ? 1 : newDNDTimeTo;
        }
      }
      
      String jsonData;
      deviceConfiguration.clear();
      JsonObject obj = deviceConfiguration.createNestedObject();
      obj[PARAM_WIFINAME] = request->hasParam(PARAM_WIFINAME) ? request->getParam(PARAM_WIFINAME)->value() : deviceConfiguration[0][PARAM_WIFINAME].as<String>();
      obj[PARAM_LAT] = newLat;
      obj[PARAM_LON] = newLon;
      obj[PARAM_SCREENSAVER] = request->hasParam(PARAM_SCREENSAVER) ? true : false;
      obj[PARAM_SCREENSAVERTIME] = newScreenSaverTime * 1000 * 60;
      obj[PARAM_SCREENSAVERTIMEOFF] = newScreenSaverTimeOff * 1000;
      obj[PARAM_DNDMODE] = request->hasParam(PARAM_DNDMODE) ? true : false;
      obj[PARAM_DNDFROM] = newDNDTimeFrom;
      obj[PARAM_DNDTO] = newDNDTimeTo;
      obj[PARAM_CELSIUS] = request->hasParam(PARAM_CELSIUS) ? true : false;
      obj[PARAM_CELSIUSSIGN] = request->hasParam(PARAM_CELSIUSSIGN) ? true : false;
      obj[PARAM_ROTATEDISPLAY] = request->hasParam(PARAM_ROTATEDISPLAY) ? true : false;
      obj[PARAM_APIKEY] = request->hasParam(PARAM_APIKEY) ? request->getParam(PARAM_APIKEY)->value() : deviceConfiguration[0][PARAM_APIKEY].as<String>();
      serializeJson(deviceConfiguration, jsonData);
    
      DEBUG_LOG_LN(jsonData.c_str());
      
      const int bytesWritten = configuration.print(jsonData.c_str());
      if (bytesWritten > 0) 
      {
        configurationUpdated = true;
        DEBUG_LOG_LN(F("File was written"));
        DEBUG_LOG_LN(bytesWritten);
        operationResult = F("Success");
        configurationUpdated = true;
      } else 
      {
          DEBUG_LOG_LN(F("File write failed"));
          operationResult = F("Failed");
      }

      if(configurationUpdated)
      {
        ApplyConfigurataion();
      }
      
      configuration.close();
    }

    DEBUG_LOG(F("Configuration saved: "));
    DEBUG_LOG_LN(operationResult);
    request->send(200, "text/text", operationResult);
  });

  webServer.onNotFound(notFound);

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

  const float lat = deviceConfiguration[0][PARAM_LAT].as<float>();
  const float lon = deviceConfiguration[0][PARAM_LON].as<float>();
  const char* metric = deviceConfiguration[0][PARAM_CELSIUS].as<bool>() ? "metric" : "imperial";
  const String apiKey = deviceConfiguration[0][PARAM_APIKEY].as<String>();

  char requestBuffer[200];
  sprintf(requestBuffer, weatherRequestURL, lat, lon, metric, apiKey.c_str());

  http.begin(client, requestBuffer);
  DEBUG_LOG_LN(requestBuffer);
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

  
  const int dndMode = deviceConfiguration[0][PARAM_DNDMODE].as<bool>();
  const int dndFrom = deviceConfiguration[0][PARAM_DNDFROM].as<int>();
  const int dndTo = deviceConfiguration[0][PARAM_DNDTO].as<int>();

  doNotDisturb = false;
  if(dndMode)
  {
    if(dndFrom > dndTo && (timeClient.getHours() >= dndFrom || timeClient.getHours() <= dndTo))
    {
      doNotDisturb = true;
    }
    else if(dndFrom < dndTo && (timeClient.getHours() >= dndFrom && timeClient.getHours() <= dndTo))
    {
      doNotDisturb = true;
    }
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
