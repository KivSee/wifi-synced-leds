#ifndef TIMESYNC_H
#define TIMESYNC_H

#include "ESPAsyncUDP.h"

class TimeSync {

public:

  TimeSync();

  void sendNTPpacket();

  void setup(const IPAddress &ntpServerAddress, uint8_t ntpServerPort);
  void loop();

// network config values
private:
  IPAddress m_address;
  uint8_t m_ntpServerPort;

private:
  void onNtpPacketCallback(AsyncUDPPacket &packet);

// timesync algorithm
private:
  unsigned long m_roundtripThresholdForUpdate = 10;

private:

  // time at which we sent last ntp packet
  uint32_t m_originTimeMillis = 0;
  bool m_originTimeValid = false;

public:
  bool m_isTimeValid = false;
  unsigned long m_startTimeSec = 0;
  unsigned long m_startTimeMillis = 0;

private:

  static const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
  uint8_t m_packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

  AsyncUDP m_udp;
};


#endif // TIMESYNC_H
