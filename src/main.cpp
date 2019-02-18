//#include <NTP.h>
#include <ESP8266WiFi.h>

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


unsigned long previousMillis = 0;        // will store last time LED was updated
int ledState = LOW;             // ledState used to set the LED

const unsigned long interval = 100;           // interval at which to blink (milliseconds)

TimeSync timeSync;

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
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Connected to wifi");

  IPAddress timeServerIP;
  WiFi.hostByName("WhereNowPi.local", timeServerIP);
  timeSync.setup(timeServerIP, 123);
}

void loop() {

  timeSync.loop();

  // delay(1000);
  unsigned long currentTime = millis();
  unsigned long currentTimeSec = (currentTime / 1000) + timeSync.startTimeSec;
  unsigned long currentTimeMillis = (currentTime % 1000) + timeSync.startTimeMillis;
  if (currentTimeMillis > 999) {
    currentTimeSec = currentTimeSec + 1;
    currentTimeMillis = currentTimeMillis % 1000;
  }

  if(currentTime - previousMillis >= 10000) {
    previousMillis = currentTime;
    printTimeUTC(currentTimeSec, currentTimeMillis);
    timeSync.sendNTPpacket();
  }

  // unsigned long currentMillis = millis();

  if(currentTimeMillis < interval) {
    digitalWrite(2,HIGH);
  }
  else {
    digitalWrite(2,LOW);
  }
}
