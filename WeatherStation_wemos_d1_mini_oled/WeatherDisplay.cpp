#include "WeatherDisplay.h"

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R1, /* reset=*/ U8X8_PIN_NONE);

CWeatherDisplay::CWeatherDisplay()
  : m_weatherInfo()
  , m_doNotDisturb(false)
  , m_isDay(true)
  , m_errorMark(false)
  , m_celsiusSign(false)
  , m_noWifiConnectionMark(false)
  , m_currentAnimationFrame(0)
  , m_needDisplayUpdate(false)
  , m_oledProtectionEnabled(false)
  , m_oledRefreshInProgress(false)
  {
    m_oledStartRefreshTimer.setInterval(WEATHER_DISPLAY_OLED_START_REFRESH);

    m_oledEndRefreshTimer.setInterval(WEATHER_DISPLAY_OLED_END_REFRESH);
    m_oledEndRefreshTimer.setRepeats(1);
  }

void CWeatherDisplay::Begin()
{
  u8g2.begin();
  u8g2.setContrast(255);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

void CWeatherDisplay::SetWeatherInfo(const SWeatherInfo& weatherInfo)
{
  m_weatherInfo = weatherInfo;
  m_needDisplayUpdate = true;
}

void CWeatherDisplay::SetDoNotDisturb(bool doNotDisturb)
{
  if(m_doNotDisturb != doNotDisturb)
  {
    m_doNotDisturb = doNotDisturb;

    if(m_doNotDisturb)
    {
      u8g2.clearBuffer();
      u8g2.sendBuffer();
      
      m_needDisplayUpdate = false;
    }
    else
    {
      m_needDisplayUpdate = true;
    }
  }
}

void CWeatherDisplay::SetIsDay(bool isDay)
{
  if(m_isDay != isDay)
  {
    m_isDay = isDay;
    m_needDisplayUpdate = true;
  }
}

void CWeatherDisplay::SetErrorMark(bool error)
{
  m_errorMark = error;
  m_needDisplayUpdate = true;
}

void CWeatherDisplay::SetNoWifiConnectionMark(bool noWifi)
{
  m_noWifiConnectionMark = noWifi;
  m_needDisplayUpdate = true;  
}

void CWeatherDisplay::SetCelsiusSign(bool celsiusSign)
{
  m_celsiusSign = celsiusSign;
}

void CWeatherDisplay::EnableOLEDProtection(bool enable, unsigned int updateTime/* = WEATHER_DISPLAY_OLED_START_REFRESH*/)
{
  m_oledProtectionEnabled = enable;
  m_oledStartRefreshTimer.setInterval(updateTime);
  if(enable)
  {
    m_oledStartRefreshTimer.start();
  }
  else
  {
    m_oledStartRefreshTimer.stop();
  }
}

void CWeatherDisplay::UpdateDisplay()
{
  if(m_doNotDisturb)
  {
    return;
  }

  if(m_oledProtectionEnabled)
  {
    InternalOledRefresh();
  }

  InternalUpdateWeatherDisplay();
}

void CWeatherDisplay::ResetAnimationFrames()
{
  m_currentAnimationFrame = 0;
}

void CWeatherDisplay::UpdateWiFiAnimation(const char* ssidName)
{
  const unsigned short offsetY = 20;
  u8g2.clearBuffer();
  u8g2.setDrawColor(0);
  u8g2.drawXBMP(0, offsetY, WIFI_ICON_W, WIFI_ICON_H, wifi_animation_64x64_bits[m_currentAnimationFrame]);
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, offsetY + WIFI_ICON_H + 10, ssidName);
  u8g2.sendBuffer();

  m_currentAnimationFrame = m_currentAnimationFrame == 3 ? 0 : ++m_currentAnimationFrame;
}

void CWeatherDisplay::UpdateWiFiConnectedState(const char* ssidName, const String& ipAdress)
{
  const unsigned short offsetY = 20;
  u8g2.clearBuffer();
  u8g2.setDrawColor(0);
  u8g2.drawXBMP(0, offsetY, WIFI_ICON_W, WIFI_ICON_H, wifi_conected_64x64_bits);
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, offsetY + WIFI_ICON_H + 10, ssidName);
  u8g2.drawStr(0, offsetY + WIFI_ICON_H + 20, ipAdress.c_str());
  u8g2.sendBuffer();
}

void CWeatherDisplay::DisplayWiFiConfigurationHelpText(const char* ssidName)
{
  const unsigned short offsetY = 20;
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, offsetY + WIFI_ICON_H / 2, ssidName);
  u8g2.sendBuffer();
}

void CWeatherDisplay::InternalUpdateWeatherDisplay()
{
  if(m_needDisplayUpdate)
  {
    m_needDisplayUpdate = false;
      
    u8g2.clearBuffer();
  
    DrawWeatherIcon(GetWeatherType(m_weatherInfo.m_weatherId, m_isDay));
    
    const unsigned short yOffset = 110;
    PrepareTemperatureForDisplay(m_weatherInfo.m_currentTemp, m_weatherInfo.m_eveningTemp, yOffset);
  
    DrawPoPBars();
  
    if(m_errorMark)
    {
      u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
      u8g2.drawStr(0, 8, "\x47");   
    }

    if(m_noWifiConnectionMark)
    {
      u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
      u8g2.drawStr(WEATHER_DISPLAY_W - 8 * 2 - 1, 8, "\x4F");
      u8g2.drawStr(WEATHER_DISPLAY_W - 8, 8, "\x50");   
    }    
    
    u8g2.sendBuffer();  
  }
}

void CWeatherDisplay::InternalOledRefresh()
{
      if(m_oledStartRefreshTimer.expired())
      {
        OledStartRefresh();
      }

      if(m_oledEndRefreshTimer.expired())
      {
        OledEndRefresh();
      }

      if(m_oledRefreshInProgress)
      {
        m_needDisplayUpdate = false;
      }
}

void CWeatherDisplay::OledStartRefresh()
{
  m_oledRefreshInProgress = true;
  m_oledEndRefreshTimer.start();

  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

void CWeatherDisplay::OledEndRefresh()
{
  m_oledRefreshInProgress = false;
  m_oledEndRefreshTimer.stop();
  m_needDisplayUpdate = true;
}

void CWeatherDisplay::DrawWeatherIcon(EWeatherType weatherType)
{
  const unsigned char* mainWeatherIcon = nullptr;
  const unsigned char* auxWeatherIcon = nullptr;
  
  switch(weatherType)
  {
  case THUNDERSTROM_LIGHT_RAIN:
  mainWeatherIcon = thunder_light_rain_56x56_bits;
  break;
  
  case THUNDERSTORM_RAIN:
  mainWeatherIcon = thunder_rain_56x56_bits;
  break;
  
  case THUNDERSTORM:
  mainWeatherIcon = thunder_rain_56x56_bits;
  break;
  
  case THUNDERSTORM_HEAVY:
  mainWeatherIcon = thunder_rain_56x56_bits;
  auxWeatherIcon = thunder_25x25_bits;
  break;
  
  case THUNDERSTORM_HEAVY_RAIN:
  mainWeatherIcon = thunder_rain_56x56_bits;
  auxWeatherIcon = more_rain_25x25_bits;
  break;


  case RAIN_LIGHT:
  mainWeatherIcon = light_rain_56x56_bits;
  break;
  
  case RAIN_LIGHT_DAY:
  mainWeatherIcon = light_rain_day_56x56_bits;
  break;
  
  case RAIN_LIGHT_NIGHT:
  mainWeatherIcon = light_rain_night_56x56_bits;
  break;
  
  case RAIN:
  mainWeatherIcon = rain_56x56_bits;
  break;
  
  case RAIN_DAY:
  mainWeatherIcon = rain_day_56x56_bits;
  break;
    
  case RAIN_NIGHT:
  mainWeatherIcon = rain_night_56x56_bits;
  break;
    
  case RAIN_HEAVY:
  mainWeatherIcon = rain_56x56_bits;
  auxWeatherIcon = more_rain_25x25_bits;
  break;
    
  case RAIN_HEAVY_DAY:
  mainWeatherIcon = rain_day_56x56_bits;
  auxWeatherIcon = more_rain_25x25_bits;
  break;
    
  case RAIN_HEAVY_NIGHT:
  mainWeatherIcon = rain_night_56x56_bits;
  auxWeatherIcon = more_rain_25x25_bits;
  break;
    
  case RAIN_FREEZING:
  mainWeatherIcon = rain_56x56_bits;
  auxWeatherIcon = snow_25x25_bits;
  break;
  
  case RAIN_FREEZING_DAY:
  mainWeatherIcon = rain_day_56x56_bits;
  auxWeatherIcon = snow_25x25_bits;
  break;
    
  case RAIN_FREEZING_NIGHT:
  mainWeatherIcon = rain_night_56x56_bits;
  auxWeatherIcon = snow_25x25_bits;
  break;
    

  case SNOW:
  mainWeatherIcon = snow_56x56_bits;
  break;
  
  case SNOW_HEAVY:
  mainWeatherIcon = snow_56x56_bits;
  auxWeatherIcon = snow_25x25_bits;
  break;
    
  case SNOW_SHOWER:
  mainWeatherIcon = snow_shower_56x56_bits;
  break;
  
  case SNOW_HEAVY_SHOWER:
  mainWeatherIcon = snow_heavy_shower_56x56_bits;
  break;
  
  case SNOW_RAIN:
  mainWeatherIcon = snow_rain_56x56_bits;
  break;
  
  case SNOW_HEAVY_RAIN:
  mainWeatherIcon = snow_heavy_rain_56x56_bits;
  break;  
  

  case MIST:
  mainWeatherIcon = mist_56x56_bits;
  break;  


  case CLEAR_DAY:
  mainWeatherIcon = sun_56x56_bits;
  break;  
  
  case CLEAR_NIGHT:
  mainWeatherIcon = night_56x56_bits;
  break;    


  case CLOUDS_LIGHT_DAY:
  mainWeatherIcon = small_clouds_day_56x56_bits;
  break;
  
  case CLOUDS_LIGHT_NIGHT:
  mainWeatherIcon = small_clouds_night_56x56_bits;
  break;
  
  case CLOUDS_MEDIUM_DAY:
  mainWeatherIcon = medium_clouds_day_56x56_bits;
  break;
  
  case CLOUDS_MEDIUM_NIGHT:
  mainWeatherIcon = medium_clouds_night_56x56_bits;
  break;
  
  case CLOUDS_HEAVY:
  mainWeatherIcon = clouds_56x56_bits;
  break;

  case UNKNOWN:
  // DEBUG OUTPUT
  break;

  default:
  // DEBUG OUTPUT
  break;
  }

  if(mainWeatherIcon || auxWeatherIcon)
  {
    u8g2.setDrawColor(0);
    
    if(mainWeatherIcon)
    {
      u8g2.drawXBMP( 4, 0, WEATHER_ICON_W, WEATHER_ICON_H, mainWeatherIcon);
    }
    else
    {
      // DEBUG OUTPUT
    }

    if(auxWeatherIcon)
    {
      u8g2.drawXBMP( WEATHER_DISPLAY_W - WEATHER_ADDITIONAL_ICON_W, 0, WEATHER_ADDITIONAL_ICON_W, WEATHER_ADDITIONAL_ICON_H, auxWeatherIcon);
    }
    else
    {
      // DEBUG OUTPUT
    }
    
    u8g2.setDrawColor(1);
  }
  else
  {
    // PRINT OUTPUT
  }
}

EWeatherType CWeatherDisplay::GetWeatherType(unsigned int weatherId, bool isDay)
{
  switch(weatherId)
  {
    // Thunderstorm
    case 200:
    return EWeatherType::THUNDERSTROM_LIGHT_RAIN;
    case 201:
    return EWeatherType::THUNDERSTORM_RAIN;
    case 202:
    return EWeatherType::THUNDERSTORM_HEAVY_RAIN;
    case 210:
    case 211:
    return EWeatherType::THUNDERSTORM;
    case 212:
    return EWeatherType::THUNDERSTORM_HEAVY;
    case 221:
    return EWeatherType::THUNDERSTORM;
    case 230:
    case 231:
    return EWeatherType::THUNDERSTROM_LIGHT_RAIN;
    case 232:
    return EWeatherType::THUNDERSTORM_HEAVY_RAIN;

    //Drizzle
    case 300:
    case 301:
    return isDay ? EWeatherType::RAIN_LIGHT_DAY : EWeatherType::RAIN_LIGHT_NIGHT;
    case 302:
    return isDay ? EWeatherType::RAIN_HEAVY_DAY : EWeatherType::RAIN_HEAVY_NIGHT;
    case 310:
    case 311:
    case 312:
    case 313:
    return isDay ? EWeatherType::RAIN_DAY : EWeatherType::RAIN_NIGHT;
    case 314:
    return isDay ? EWeatherType::RAIN_HEAVY_DAY : EWeatherType::RAIN_HEAVY_NIGHT;
    case 321:
    return isDay ? EWeatherType::RAIN_DAY : EWeatherType::RAIN_NIGHT;

    // Rain
    case 500:
    return isDay ? EWeatherType::RAIN_LIGHT_DAY : EWeatherType::RAIN_LIGHT_NIGHT;
    case 501:
    return isDay ? EWeatherType::RAIN_DAY : EWeatherType::RAIN_NIGHT;
    case 502:
    case 503:
    case 504:
    return isDay ? EWeatherType::RAIN_HEAVY_DAY : EWeatherType::RAIN_HEAVY_NIGHT;
    case 511:
    return isDay ? EWeatherType::RAIN_FREEZING_DAY : EWeatherType::RAIN_FREEZING_NIGHT;
    case 520:
    case 521:
    return isDay ? EWeatherType::RAIN_DAY : EWeatherType::RAIN_NIGHT;
    case 522:
    return isDay ? EWeatherType::RAIN_HEAVY_DAY : EWeatherType::RAIN_HEAVY_NIGHT;
    case 531:
    return isDay ? EWeatherType::RAIN_DAY : EWeatherType::RAIN_NIGHT;

    // Snow
    case 600:
    case 601:
    return EWeatherType::SNOW;
    case 602:
    return EWeatherType::SNOW_HEAVY;
    case 611:
    case 612:
    return EWeatherType::SNOW_SHOWER;
    case 613:
    return EWeatherType::SNOW_HEAVY_SHOWER;
    case 615:
    return EWeatherType::SNOW_RAIN;
    case 616:
    return EWeatherType::SNOW_HEAVY_RAIN;
    case 620:
    return EWeatherType::SNOW_SHOWER;
    case 621:
    case 622:
    return EWeatherType::SNOW_HEAVY_SHOWER;

    //Atmosphere
    case 701:
    case 711:
    case 721:
    case 731:
    case 741:
    case 751:
    case 761:
    case 762:
    case 771:
    case 781:
    return EWeatherType::MIST;

    //Clear
    case 800:
    return isDay ? EWeatherType::CLEAR_DAY : EWeatherType::CLEAR_NIGHT;

    //Clouds
    case 801:
    return isDay ? EWeatherType::CLOUDS_LIGHT_DAY : EWeatherType::CLOUDS_LIGHT_NIGHT;
    case 802:
    case 803:
    return isDay ? EWeatherType::CLOUDS_MEDIUM_DAY : EWeatherType::CLOUDS_MEDIUM_NIGHT;
    case 804:
    return EWeatherType::CLOUDS_HEAVY;

    default:
    return EWeatherType::UNKNOWN;
  }
}

void CWeatherDisplay::PrepareTemperatureForDisplay(const short currentTemp, const short eveningTemp, const unsigned short yOffset)
{
  const unsigned short currenttemperatureCursorOffsetX = 0;
  const unsigned short currentTemperatureCursorOffsetY = yOffset;
  const unsigned short currentTemperatureTextSize = 3;
  
  const unsigned short eveningTemperatureCursorOffsetX = 30;
  const unsigned short eveningTemperatureCursorOffsetY = 17;
  const unsigned short eveningTemperatureTextSize = 1;

  u8g2.setFont(u8g2_font_fub30_tn);
  
  u8g2.setCursor(currenttemperatureCursorOffsetX,currentTemperatureCursorOffsetY);
  
  DisplayTemperatureAlligment(currentTemp);

  if(m_celsiusSign)
  {
    // Display Celsius sign
    u8g2.setFont(u8g2_font_helvR12_tf);
    
    u8g2.setCursor(currenttemperatureCursorOffsetX + 58,currentTemperatureCursorOffsetY - 23);
    u8g2.print("\xb0");
  }

  u8g2.setFont(u8g2_font_fub14_tn);
  u8g2.setCursor(eveningTemperatureCursorOffsetX,currentTemperatureCursorOffsetY + eveningTemperatureCursorOffsetY);
  DisplayTemperatureAlligment(eveningTemp);
}

void CWeatherDisplay::DisplayTemperatureAlligment(const short temp)
{
  const bool minus = temp < 0 ? true : false;
  short tens = temp / 10;
  tens = tens < 0 ? tens * -1 : tens;
  short numbers = temp < 0 ? temp * -1 : temp;
  numbers -= tens * 10;
    
  if(minus)
  {
    u8g2.print("-");
  }
  else
  {
    u8g2.print(" ");
  }
    
  if(tens != 0)
  {
    u8g2.print(tens);
  }
  else
  {
    u8g2.print(" ");
  }
    
  u8g2.print(numbers);
}

void CWeatherDisplay::DrawPoPBars()
{
  unsigned short offsetX = 0;
  const unsigned short offsetY = 57;

  const unsigned short gap = 2;
  const unsigned short barWidth = 2;

  for(unsigned short index = 0, offsetX = 0; offsetX <= WEATHER_DISPLAY_W; offsetX += barWidth + gap, ++index)
  {
    DrawBar(offsetX, offsetY, barWidth, m_weatherInfo.m_pop[index]);
  }  
}

void CWeatherDisplay::DrawBar(const unsigned short barPosX, const unsigned short barPosY, unsigned short barWidth, float barHeightPercent) 
{
  const unsigned short maxBarHeightPx = 16;
  const float maxBarHeightPercent = 1.0f;

  unsigned barHeightPx = 0;

  barHeightPercent = barHeightPercent > maxBarHeightPercent ? maxBarHeightPercent : barHeightPercent;

  barHeightPx = ceil(maxBarHeightPx * barHeightPercent);
  
  u8g2.drawBox(barPosX, barPosY, barWidth, barHeightPx);
}
