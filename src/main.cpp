#include <Arduino.h>

#include "config.h"

extern "C"
{
#include "gfx.h"
}

#include <cstring>
#include <memory>
#include <HTTPClient.h>
#include <time.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include <ESPAsyncWiFiManager.h>

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "rom/rtc.h"

#include <ArduinoJson.h>

#include "SPIFFS.h"

struct Config
{
  char imageUrl[64] = "";
};

const uint64_t uS_TO_S_FACTOR = 1000000;
const uint64_t S_TO_H_FACTOR = 3600;
const uint64_t H_TO_SLEEP = 1;

const char *ntpServer0 = "0.pool.ntp.org";
const char *ntpServer1 = "1.pool.ntp.org";
const char *ntpServer2 = "2.pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

GDisplay *display;

AsyncWebServer *server;

Config config;

std::unique_ptr<uint8_t[]> fetchImage(const String &url)
{
  HTTPClient https;

  Serial.println(url);

  if (!https.begin(url))
  {
    Serial.println(F("[HTTP] Can not establish connection to Server."));
    throw std::logic_error("Can not establish HTTP connection!");
  }

  int httpCode = https.GET();
  Serial.print(F("[HTTP] GET... code: "));
  Serial.println(httpCode);

  if (httpCode != HTTP_CODE_OK)
  {
    Serial.print(F("[HTTP] GET... failed, error: "));
    Serial.println(https.errorToString(httpCode));
    https.end();
    throw std::logic_error("HTTP Code not OK");
  }

  int total = https.getSize();
  int remaining = total;

  uint8_t buffer[128] = {0};
  std::unique_ptr<uint8_t[]> destination{new uint8_t[total]};
  int position = 0;

  WiFiClient *stream = https.getStreamPtr();

  while (https.connected() && (remaining > 0 || remaining == -1))
  {
    size_t size = stream->available();

    if (size)
    {
      int count = stream->readBytes(buffer, (size > sizeof(buffer)) ? sizeof(buffer) : size);
      if (position + count <= total)
      {
        std::memcpy(destination.get() + position, &buffer, count);
      }
      else
      {
        Serial.print(F("[HTTP] Got too much data for destination!"));
        Serial.print(String("got ") + String(position + count - total) + " bytes too much\n");
        throw std::out_of_range("Got too much data!");
      }
      position += count;
      if (remaining > 0)
      {
        remaining -= count;
      }
    }
    delay(1);
  }

  Serial.println(F("[HTTP] Finished Download"));
  https.end();
  return std::move(destination);
}

bool connectToWifi()
{
  server = new AsyncWebServer(80);
  DNSServer dns;
  AsyncWiFiManager wifiManager(server, &dns);
  // wifiManager.resetSettings();
  wifiManager.autoConnect("E-Paper Pictureframe");
  delete server;
  return true;
}

void showImage(void *data, coord_t startX, coord_t startY)
{
  gdispImage image;
  GFILE *imageData = gfileOpenMemory(data, "rb");
  gdispImageError err = gdispImageOpenGFile(&image, imageData);

  if (err)
  {
    Serial.println("Error opening Image");
    Serial.println(err, HEX);
    gfileClose(imageData);
    ESP.restart();
    return;
  }

  // render image
  coord_t imageStartX = startX;
  coord_t imageStartY = startY;

  gdispImageDraw(&image, imageStartX, imageStartY, image.width, image.height, 0, 0);
  gdispImageClose(&image);
  gfileClose(imageData);
}

void loadImage(const String &url, std::unique_ptr<uint8_t[]> &image)
{
  Serial.println("fetch image");
  try
  {
    image = fetchImage(url);
  }
  catch (const std::logic_error &e)
  {
    Serial.println(F("Could not fetch image"));
    Serial.println(e.what());
    ESP.restart();
  }
}

void draw()
{
  std::unique_ptr<uint8_t[]> image;
  loadImage(config.imageUrl, image);

  Serial.println("start drawing");
  gfxInit();
  GDisplay *display = gdispGetDisplay(0);
  gdispSetOrientation(GDISP_ROTATE_180);
  auto text = ":D";
  font_t font = gdispOpenFont("DejaVuSans20");
  gdispGDrawString(display, 100, 100, text, font, GFX_BLACK);
  gdispCloseFont(font);

  showImage(static_cast<void *>(image.get()), 0, 0);
  gdispGFlush(display);

  Serial.println("end");
}

void updateTime()
{
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer0, ntpServer1, ntpServer2);
  sntp_sync_time(NULL);
}

uint64_t getSleepTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    ESP.restart();
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  uint64_t hours = timeinfo.tm_hour;
  uint64_t minutes = timeinfo.tm_min;
  uint64_t seconds = timeinfo.tm_sec;
  Serial.println(hours);
  Serial.println(minutes);
  Serial.println(seconds);

  auto time_us = ((/*hours * 60ull + */ minutes % 5ull) * 60ull + seconds) * 1000000ull;
  auto sleepTime = (/*24ull * */ 5ull * 60ull * 1000000ull) - time_us;

  Serial.println(time_us);
  Serial.println(sleepTime);

  Serial.print("going to sleep for ");
  Serial.println(sleepTime);
  return sleepTime;
}

void print_reset_reason(RESET_REASON reason)
{
  switch (reason)
  {
  case 1:
    Serial.println("POWERON_RESET");
    break; /**<1,  Vbat power on reset*/
  case 3:
    Serial.println("SW_RESET");
    break; /**<3,  Software reset digital core*/
  case 4:
    Serial.println("OWDT_RESET");
    break; /**<4,  Legacy watch dog reset digital core*/
  case 5:
    Serial.println("DEEPSLEEP_RESET");
    break; /**<5,  Deep Sleep reset digital core*/
  case 6:
    Serial.println("SDIO_RESET");
    break; /**<6,  Reset by SLC module, reset digital core*/
  case 7:
    Serial.println("TG0WDT_SYS_RESET");
    break; /**<7,  Timer Group0 Watch dog reset digital core*/
  case 8:
    Serial.println("TG1WDT_SYS_RESET");
    break; /**<8,  Timer Group1 Watch dog reset digital core*/
  case 9:
    Serial.println("RTCWDT_SYS_RESET");
    break; /**<9,  RTC Watch dog Reset digital core*/
  case 10:
    Serial.println("INTRUSION_RESET");
    break; /**<10, Instrusion tested to reset CPU*/
  case 11:
    Serial.println("TGWDT_CPU_RESET");
    break; /**<11, Time Group reset CPU*/
  case 12:
    Serial.println("SW_CPU_RESET");
    break; /**<12, Software reset CPU*/
  case 13:
    Serial.println("RTCWDT_CPU_RESET");
    break; /**<13, RTC Watch dog Reset CPU*/
  case 14:
    Serial.println("EXT_CPU_RESET");
    break; /**<14, for APP CPU, reseted by PRO CPU*/
  case 15:
    Serial.println("RTCWDT_BROWN_OUT_RESET");
    break; /**<15, Reset when the vdd voltage is not stable*/
  case 16:
    Serial.println("RTCWDT_RTC_RESET");
    break; /**<16, RTC Watch dog reset digital core and rtc module*/
  default:
    Serial.println("NO_MEAN");
  }
}

void sleep()
{
  auto sleepTime = getSleepTime();
  esp_sleep_enable_timer_wakeup(sleepTime);
  esp_deep_sleep_start();
}

void showIp()
{
  gfxInit();
  GDisplay *display = gdispGetDisplay(0);
  gdispSetOrientation(GDISP_ROTATE_180);
  auto text = ("ip: " + WiFi.localIP().toString()).c_str();
  font_t font = gdispOpenFont("DejaVuSans20");
  gdispGDrawString(display, 100, 100, text, font, GFX_BLACK);
  gdispCloseFont(font);
  gdispGFlush(display);
}

String readFile(const char *fileName)
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return "An Error has occurred while mounting SPIFFS";
  }

  File file = SPIFFS.open(fileName);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return "Failed to open file for reading";
  }

  Serial.println("File Content:");
  auto content = file.readString();
  Serial.println(content);
  file.close();
  return content;
}

void loadConfiguration(const char *fileName, Config &config)
{

  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  // Open file for reading
  File file = SPIFFS.open(fileName);

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
    Serial.println(F("Failed to read file, using default configuration"));

  // Copy values from the JsonDocument to the Config
  strlcpy(config.imageUrl,                 // <- destination
          doc["imageUrl"] | "example.com", // <- source
          sizeof(config.imageUrl));        // <- destination's capacity

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
}

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin(115200);
  delay(10);
  connectToWifi();

  loadConfiguration("/config.json", config);
  Serial.print("imageUrl: ");
  Serial.println(config.imageUrl);

  print_reset_reason(rtc_get_reset_reason(0));
  if (rtc_get_reset_reason(0) == POWERON_RESET)
  {
    server = new AsyncWebServer(80);
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/html", readFile("/index.html"));
    });

    AsyncElegantOTA.begin(server); // Start ElegantOTA
    server->begin();

    showIp();

    auto start = millis();
    while (millis() < start + 2 * 60 * 1000)
    {
      AsyncElegantOTA.loop();
    }
    ESP.restart();
  }
  else
  {
    updateTime();
    draw();
    sleep();
  }
}

void loop()
{
}