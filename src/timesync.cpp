
#include "timesync.hpp"
#include "Arduino.h" // for millis() function

TimeSync::TimeSync() {
  // set all bytes in the buffer to 0
  memset(m_packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  m_packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  m_packetBuffer[1] = 0;     // Stratum, or type of clock
  m_packetBuffer[2] = 6;     // Polling Interval
  m_packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  m_packetBuffer[12]  = 49;
  m_packetBuffer[13]  = 0x4E;
  m_packetBuffer[14]  = 49;
  m_packetBuffer[15]  = 52;
}

void TimeSync::sendNTPpacket() {

  m_originTimeMillis = millis();

  // all NTP fields have been given values, now
  // we can send a packet requesting a timestamp:
  m_udp.writeTo(m_packetBuffer, NTP_PACKET_SIZE, m_address, m_ntpServerPort);
}

void TimeSync::onNtpPacketCallback(AsyncUDPPacket &packet)
{
  // stamp the recv time, this is important to be done ASAP
  // e.g - from the interrupt handler, and not the loop
  m_recvTime = millis();

  // parse ntp response buffer
  uint8_t *packetBuffer = (uint8_t *)packet.data();
  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
  m_secFromNtpEpoch = highWord << 16 | lowWord;
  unsigned long otherHighWord = word(packetBuffer[44], packetBuffer[45]);
  unsigned long otherLowWord = word(packetBuffer[46], packetBuffer[47]);
  unsigned long fractional = otherHighWord << 16 | otherLowWord;
  float read_msF = ((float)fractional)*2.3283064365387E-07; // fractional*(1000/2^32) to get milliseconds.
  m_msFromNtpEpoch=(word)(read_msF);

  m_roundTrip = m_recvTime - m_originTimeMillis;
}

void TimeSync::setup(const IPAddress &ntpServerAddress, uint8_t ntpServerPort) {

  m_address = ntpServerAddress;
  m_ntpServerPort = ntpServerPort;

  if(m_udp.connect(m_address, m_ntpServerPort)) {
    Serial.println("UDP connected");
    AuPacketHandlerFunction callback = std::bind(&TimeSync::onNtpPacketCallback, this, std::placeholders::_1);
    m_udp.onPacket(callback);
  }
}

void TimeSync::loop() {

  if (m_roundTrip < 10) {
    Serial.print("round trip is "); Serial.print(m_roundTrip);Serial.println(" ms, updating internal time");

    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    unsigned long NTPsyncTimeSec = m_secFromNtpEpoch - seventyYears;
    unsigned long NTPsyncTimeMillis = m_msFromNtpEpoch;
    unsigned long recvTimeSec = m_recvTime / 1000;
    unsigned long recvTimeMillis = m_recvTime % 1000;
    startTimeSec = NTPsyncTimeSec - recvTimeSec;
    if ((NTPsyncTimeMillis - recvTimeMillis) < 0) {
      startTimeMillis = 1000 - (recvTimeMillis - NTPsyncTimeMillis);
      startTimeSec = startTimeSec - 1;
    }
    else {
      startTimeMillis = NTPsyncTimeMillis - recvTimeMillis;
    }
    m_roundTrip = 999;
  }

}
