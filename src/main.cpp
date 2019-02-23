#define ARDUINOJSON_USE_LONG_LONG 1
//#include <NTP.h>
#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "timesync.hpp"

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
  pinMode(2, OUTPUT);

  Serial.begin(115200);
  Serial.println();
  startWiFi();

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Connected to wifi");

  IPAddress songServerIP(10,0,0,37);
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
}

void loop() {

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
    if ((((songCurrentTime/1000)%10 == 0) && (songCurrentTime%1000 < 500))) {
      digitalWrite(2,HIGH);
      Serial.print("songStartTime: "); Serial.println(songStartTime);
      Serial.print("currentTim: "); Serial.println(currentTim);
      Serial.print(songCurrentTime/1000);Serial.print(".");Serial.println(songCurrentTime%1000);
    }
    else if (songCurrentTime%1000 < 100) {
      digitalWrite(2,HIGH);
      Serial.print("songStartTime: "); Serial.println(songStartTime);
      Serial.print("currentTim: "); Serial.println(currentTim);
      Serial.print(songCurrentTime/1000);Serial.print(".");Serial.println(songCurrentTime%1000);
    }
    else {
      digitalWrite(2,LOW);
    }
  }

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
