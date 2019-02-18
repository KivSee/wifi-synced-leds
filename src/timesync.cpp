
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
  m_originTimeValid = true;

  // all NTP fields have been given values, now
  // we can send a packet requesting a timestamp:
  m_udp.writeTo(m_packetBuffer, NTP_PACKET_SIZE, m_address, m_ntpServerPort);
}

void TimeSync::onNtpPacketCallback(AsyncUDPPacket &packet)
{
  // this might be a retransmission of a packet we already received,
  // or some other network issue which we cannot handle
  if(!m_originTimeValid) {
    return;
  }

  // stamp the recv time, this is important to be done ASAP
  uint32_t recvTime = millis();

  // check if time update is needed
  unsigned int roundTrip = recvTime - m_originTimeMillis;
  m_originTimeValid = false;
  if(roundTrip >= m_roundtripThresholdForUpdate) {
    // this packet took too much time for round trip. we don't use it
    return;
  }

  Serial.print("round trip is "); Serial.print(roundTrip); Serial.println(" ms, updating internal time");

  // parse ntp response buffer
  uint8_t *packetBuffer = packet.data();

  // seconds part
  uint32_t highWord = word(packetBuffer[40], packetBuffer[41]);
  uint32_t lowWord = word(packetBuffer[42], packetBuffer[43]);
  uint32_t secFromNtpEpoch = highWord << 16 | lowWord;
  // ms part
  uint32_t otherHighWord = word(packetBuffer[44], packetBuffer[45]);
  uint32_t otherLowWord = word(packetBuffer[46], packetBuffer[47]);
  uint32_t fractional = otherHighWord << 16 | otherLowWord;
  float readMsF = ((float)fractional)*2.3283064365387E-07; // fractional*(1000/2^32) to get milliseconds.
  if(readMsF < 0.0) {
    // should not happen, but we will check just in case
    readMsF = 0.0;
  }
  // this is just the fractional part. should be in range [0, 1000)
  uint32_t msPart = (uint32_t)(readMsF);
  if(msPart >= 1000) {
    // this can happen in very rare scenarions, because float calculations are
    // not precice and can inject errors
    msPart = 999;
  }

  // Unix time starts on Jan 1 1970. ntp epoch is Jan 1 1900,
  // In seconds, that's 2208988800:
  static const unsigned long seventyYears = 2208988800UL;
  uint32_t secFromEpoch = secFromNtpEpoch - seventyYears;

  // esp epoch is the time when esp started. it is the time for which millis()
  // function returned 0.
  // now we will calculate what was the time (seconds + ms) on esp epoch.
  // that is done by reducing 'recvTime' from the time sent from ntp server.

  unsigned long recvTimeSec = recvTime / 1000;
  unsigned long recvTimeMillis = recvTime % 1000;
  m_startTimeSec = secFromEpoch - recvTimeSec;
  if ((msPart - recvTimeMillis) < 0) {
    m_startTimeMillis = 1000 - (recvTimeMillis - msPart);
    m_startTimeSec--;
  }
  else {
    m_startTimeMillis = msPart - recvTimeMillis;
  }
  m_isTimeValid = true;

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

}
