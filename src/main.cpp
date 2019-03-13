#define ARDUINOJSON_USE_LONG_LONG 1
#define FASTLED_ESP8266_DMA

#include "FastLED.h"
#include <ESP8266WiFi.h>
// #include <ESP8266mDNS.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include "timesync.hpp"

#if FASTLED_VERSION < 3001000
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define DATA_PIN    9
//#define CLK_PIN   4
#define LED_TYPE    WS2811
#define COLOR_ORDER RGB
#define NUM_LEDS    50
#define BRIGHTNESS  100

/*
We don't want to have private ssid and passwords pushed to git.
It is not safe, and every setup has it's own.

Therfore, each clone should define it's one credentials in a separate file.
Create a file named credentials.h under include directory,
and add the following text to it:

#ifndef CREDENTIALS_H
#define CREDENTIALS_H

// Replace with your actual SSID and password:
#define WIFI_SSID "Your SSID here"
#define WIFI_PASSWD "Your password here"

#endif
*/
#include "credentials.h"

CRGB leds[NUM_LEDS];
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

int ledState = LOW;             // ledState used to set the LED

const unsigned long interval = 100;           // interval at which to blink (milliseconds)

TimeSync timeSync;
WebSocketsClient webSocket;
bool isInSong = false;
// char* filename;
uint64_t startTimeFromPlayer = 0;
// float speed;

#define USE_SERIAL Serial
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
	switch(type) {
		case WStype_DISCONNECTED:
			USE_SERIAL.printf("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED: {
			USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);
			// send message to server when Connected
			webSocket.sendTXT("Connected");
		}
			break;
		case WStype_TEXT:
			USE_SERIAL.printf("[WSc] get text: %s\n", payload);
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(payload);
      // filename = root["file_name"];
      isInSong = root["song_is_playing"].as<bool>();
      startTimeFromPlayer = root["start_time_millis_since_epoch"].as<uint64_t>();
      uint32_t low = startTimeFromPlayer % 0xFFFFFFFF;
      uint32_t high = (startTimeFromPlayer >> 32) % 0xFFFFFFFF;
      USE_SERIAL.println("got start_time_millis_since_epoch from player");
      USE_SERIAL.print("low: ");
      USE_SERIAL.println(low);
      USE_SERIAL.print("high: ");
      USE_SERIAL.println(high);
      // speed = root["speed"];
			// send message to server
			// webSocket.sendTXT("message here");
			break;
		// case WStype_BIN:
		// 	USE_SERIAL.printf("[WSc] get binary length: %u\n", length);
		// 	hexdump(payload, length);
		// 	// send data to server
		// 	// webSocket.sendBIN(payload, length);
		// 	break;
	}
}

void startWiFi(void)
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
}

void printTimeUTC (unsigned long timeFromEpochSec, unsigned long timeFromEpochMillis) {
  // print the hour, minute and second:
  Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
  Serial.print((timeFromEpochSec  % 86400L) / 3600); // print the hour (86400 equals secs per day)
  Serial.print(':');
  if (((timeFromEpochSec % 3600) / 60) < 10) {
    // In the first 10 minutes of each hour, we'll want a leading '0'
    Serial.print('0');
  }
  Serial.print((timeFromEpochSec  % 3600) / 60); // print the minute (3600 equals secs per minute)
  Serial.print(':');
  if ((timeFromEpochSec % 60) < 10) {
    // In the first 10 seconds of each minute, we'll want a leading '0'
    Serial.print('0');
  }
  Serial.print(timeFromEpochSec % 60); // print the second
  Serial.print('.');
  if (timeFromEpochMillis < 100) {
    Serial.print('0');
    if (timeFromEpochMillis < 10) {
      Serial.print('0');
    }
  }
  Serial.println(timeFromEpochMillis);
}

void setup() {
  // Start serial output, do not start after FastLED init!
  Serial.begin(115200);
  delay(100);
  Serial.setDebugOutput(true);

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS)
         .setCorrection(TypicalLEDStrip)
         .setDither(BRIGHTNESS < 255);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

  pinMode(2, OUTPUT);


  startWiFi();

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Connected to wifi");

  IPAddress songServerIP(10,0,0,102);
  IPAddress timeServerIP(10,0,0,102);
  // IPAddress timeServerIP;
  // WiFi.hostByName("WhereNowPi.local", timeServerIP);
  while (timeServerIP == IPAddress(0,0,0,0)) {
    Serial.println(timeServerIP);
    delay(1000);
    WiFi.hostByName("WhereNowPi.local", timeServerIP);
  }
  Serial.print("Connecting to Time Server IP: ");
  Serial.println(timeServerIP);
  timeSync.setup(timeServerIP, 123);

  // server address, port and URL
	webSocket.begin(songServerIP, 9002, "/");
	// event handler
	webSocket.onEvent(webSocketEvent);
  // try every 5000 again if connection has failed
	webSocket.setReconnectInterval(5000);
  // start heartbeat (optional)
  // ping server every 15000 ms
  // expect pong from server within 3000 ms
  // consider connection disconnected if pong is not received 2 times
  webSocket.enableHeartbeat(15000, 3000, 2);


  /////// ADDED SECTION FOR OTA ///////////
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname(host);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA setup sequence finished");
}

void loop() {
  ArduinoOTA.handle();
  timeSync.loop();

  if(!timeSync.m_isTimeValid) {
    return;
  }

  if (isInSong) {
    int64_t espStartTime = (int64_t)(((uint64)timeSync.m_startTimeSec)*1000 + timeSync.m_startTimeMillis);
    // when esp miilis() function equaled songStartTime -> song had offset 0
    int32_t songStartTime = (int32_t)((int64_t)startTimeFromPlayer - espStartTime); // can this be int32_t??
    unsigned long currentTim = millis();
    uint32_t songCurrentTime = currentTim - songStartTime;
		uint32_t songCurrentTimeMillis = songCurrentTime%1000;
    uint32_t songCurrentTimeSecs = songCurrentTime/1000;
    if (((songCurrentTimeSecs%10 == 0) && (songCurrentTimeMillis < 500))) {
      digitalWrite(2,HIGH);
      // Serial.print("songStartTime: "); Serial.println(songStartTime);
      // Serial.print("currentTim: "); Serial.println(currentTim);
      // Serial.print(songCurrentTimeSecs);Serial.print(".");Serial.println(songCurrentTimeMillis);
			gHue = 0;
    }
    else if (songCurrentTimeMillis < 100) {
      digitalWrite(2,HIGH);
      // Serial.print("songStartTime: "); Serial.println(songStartTime);
      // Serial.print("currentTim: "); Serial.println(currentTim);
      // Serial.print(songCurrentTimeSecs);Serial.print(".");Serial.println(songCurrentTimeMillis);
			gHue = 0;
    }
    else {
      digitalWrite(2,LOW);
			leds[0] = CRGB::Red;
			float millisFloat = (float)(songCurrentTimeMillis - 100) / 900.0;
			gHue = (uint8_t)(millisFloat * 255);
			// Serial.println(gHue);
    }
  }

	// FastLED's built-in rainbow generator
  // fill_rainbow( leds, NUM_LEDS, gHue, 255 / NUM_LEDS);
  fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
  FastLED.show();
  // delay(10);
  // gHue++;

  // unsigned long currentTime = millis();
  // unsigned long currentTimeSec = (currentTime / 1000) + timeSync.m_startTimeSec;
  // unsigned long currentTimeMillis = (currentTime % 1000) + timeSync.m_startTimeMillis;
  // if (currentTimeMillis > 999) {
  //   currentTimeSec = currentTimeSec + 1;
  //   currentTimeMillis = currentTimeMillis % 1000;
  // }
  //
  // if(currentTimeMillis < interval) {
  //   digitalWrite(2,HIGH);
  // }
  // else {
  //   digitalWrite(2,LOW);
  // }

  webSocket.loop();
}
