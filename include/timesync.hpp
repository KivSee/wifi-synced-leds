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

private:
  // time at which we sent last ntp packet
  unsigned long m_originTimeMillis = 0;
  // round trip time of last ntp packet
  unsigned long m_roundTrip = 999;
  // time in arduino clock (millis) when packet was recevied
  unsigned long m_recvTime = 0;

  // following values are the response time from the ntp server.
  // ntp response with values from 1 Jan 1900 (not 1970)
  unsigned long m_secFromNtpEpoch = 0; // seconds since epoch
  unsigned long m_msFromNtpEpoch = 0; // ms part, e.g. [0, 1000)

public:
  unsigned long startTimeSec = 0;
  unsigned long startTimeMillis = 0;

private:

  static const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
  uint8_t m_packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

  AsyncUDP m_udp;
};


#endif // TIMESYNC_H
