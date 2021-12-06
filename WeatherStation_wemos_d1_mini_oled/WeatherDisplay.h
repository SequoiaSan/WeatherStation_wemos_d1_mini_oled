#ifndef _WEATHERDISPLAY_H
#define _WEATHERDISPLAY_H

#include <Arduino.h>
#include <Array.h>
#include <U8g2lib.h>
#include <MillisTimer.h>

#include "weather_icons.h"
#include "wifi_icons.h"

#include "DebugHelpers.h"

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

///////////////// DEFINES
#define DISPLAY_CELSIUS 1

#ifdef DEBUG
#define WEATHER_DISPLAY_OLED_START_REFRESH 1000 * 30
#define WEATHER_DISPLAY_OLED_END_REFRESH 1000 * 2
#else
#define WEATHER_DISPLAY_OLED_START_REFRESH 1000 * 60 * 20
#define WEATHER_DISPLAY_OLED_END_REFRESH 1000 * 4
#endif

#define WEATHER_DISPLAY_W 64
#define WEATHER_DISPLAY_H 128

///////////////// CODE
enum EWeatherType
{
  UNKNOWN = -1,
  
  THUNDERSTROM_LIGHT_RAIN = 0,
  THUNDERSTORM_RAIN,
  THUNDERSTORM,
  THUNDERSTORM_HEAVY,
  THUNDERSTORM_HEAVY_RAIN,

  RAIN_LIGHT,
  RAIN_LIGHT_DAY,
  RAIN_LIGHT_NIGHT,
  RAIN,
  RAIN_DAY,
  RAIN_NIGHT,
  RAIN_HEAVY,
  RAIN_HEAVY_DAY,
  RAIN_HEAVY_NIGHT,
  RAIN_FREEZING,
  RAIN_FREEZING_DAY,
  RAIN_FREEZING_NIGHT,

  SNOW,
  SNOW_HEAVY,
  SNOW_SHOWER,
  SNOW_HEAVY_SHOWER,
  SNOW_RAIN,
  SNOW_HEAVY_RAIN,

  MIST,

  CLEAR_DAY,
  CLEAR_NIGHT,

  CLOUDS_LIGHT_DAY,
  CLOUDS_LIGHT_NIGHT,
  CLOUDS_MEDIUM_DAY,
  CLOUDS_MEDIUM_NIGHT,
  CLOUDS_HEAVY
};

struct SWeatherInfo
{
  unsigned int m_weatherId = 0;
  Array<float, WEATHER_DISPLAY_W> m_pop;
  short m_currentTemp = 0;
  short m_eveningTemp = 0;
};

class CWeatherDisplay
{
  public:
    CWeatherDisplay();

    void Begin();
    void SetWeatherInfo(const SWeatherInfo& weatherInfo);
    void SetDoNotDisturb(bool doNotDisturb);
    void SetIsDay(bool isDay);
    void SetErrorMark(bool error);
    void SetNoWifiConnectionMark(bool noWifi);

    void EnableOLEDProtection(bool enable, unsigned int updateTime = WEATHER_DISPLAY_OLED_START_REFRESH);

    void UpdateDisplay();

    void ResetAnimationFrames();
    void UpdateWiFiAnimation(const char* ssidName);
    void UpdateWiFiConnectedState(const char* ssidName, const String& ipAdress);

    void DisplayWiFiConfigurationHelpText(const char* ssidName);
    
  private:
    void InternalUpdateWeatherDisplay();
    void InternalOledRefresh();
    void DrawWeatherIcon(EWeatherType weatherType);
    void PrepareTemperatureForDisplay(const short currentTemp, const short eveningTemp, const unsigned short yOffset);
    void DisplayTemperatureAlligment(const short temp);
    void DrawBar(const unsigned short barPosX, const unsigned short barPosY, unsigned short barWidth, float barHeightPercent);
    void DrawPoPBars();

    void OledStartRefresh();
    void OledEndRefresh();

    EWeatherType GetWeatherType(unsigned int weatherId, bool isDay);
    
  private:
    SWeatherInfo m_weatherInfo;
    bool m_doNotDisturb;
    bool m_isDay;
    bool m_errorMark;
    bool m_noWifiConnectionMark;
    bool m_needDisplayUpdate;
    bool m_oledProtectionEnabled;
    bool m_oledRefreshInProgress;

    MillisTimer m_oledStartRefreshTimer;
    MillisTimer m_oledEndRefreshTimer;

    unsigned short m_currentAnimationFrame;
};
#endif
