/*
 * Copyright (c) 2022 Subreption LLC. All rights reserved.
 * Author: 12dc8242df1be0d6f4b73d68166552197936a27074c004c8af95b27194ec584d
 *
 * Dual-licensed under the Subreption Ukraine Defense License (SUDL, version 1) and the  Server Side
 * Public License (SSPL, version 3). Both licenses are provided with this software distribution.
 */

#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include "rapidjson/document.h"
#include <el3dec/utils.hpp>

#define EL3DEC_VERSION  1

#define ENICS_ELERON_PACKET_MAGICBYTE 0xAA
#define ENICS_ELERON_PACKET_TELEMETRY 0x0F

struct GpsLocation {
  float latitude;
  float longitude;
  uint16_t altitude;
};

struct El3CameraSetting {
  float angle;
  float position;
  float azimuth;
};

enum El3DecOpMode {
  FAULT_TOLERANT,
  FAULT_INTOLERANT
};

class El3Telemetry
{
  public:
    El3Telemetry(const unsigned char *buf, const size_t len, El3DecOpMode opmode);
    ~El3Telemetry();

    std::string toJson(bool pretty);

    float Latitude() { return gpsData.latitude; }
    float Longitude() { return gpsData.longitude; }
    float Altitude() { return gpsData.altitude; }
    int ID() { return uavNo; }
    int Type() { return uavType; }
    float Groundspeed() { return groundSpeed; }
    int VideoFreq() { return videoTxFreq; }
    int RemainingFlightMinutes() { return remainingMinutes; }
    float CameraPosition() { return camera.position; }
    float CameraAzimuth() { return camera.azimuth; }
    float CameraAngle() { return camera.angle; }

    std::string Timestamp() {
      return string_format("%d:%d:%d", stampHours, stampMinutes, stampSeconds);
    }

  private:
    void parseRaw();
    void parseTimestamp();
    void parseFlightData();
    void parseVideoParams();

    void parsingDone();

    bool checkReadBufferSanity(size_t offset, size_t toread);

    float getPackedCoordinate(size_t off, size_t msboff, int bitshift);

  protected:
    uint8_t magicByte;
    uint8_t dataLength;
    uint8_t packetType;
    uint8_t engineType;
    uint8_t uavType;
    uint16_t uavNo;

    uint16_t flightTime;

    /* timestamp */
    uint8_t stampHours;
    uint8_t stampMinutes;
    uint8_t stampSeconds;

    /* flight information */
    struct GpsLocation gpsData;
    float groundSpeed;
    float careen;
    float pitch;
    uint16_t remainingMinutes;

    /* video settings */
    uint16_t videoTxChannel;
    uint16_t videoTxFreq;
    struct El3CameraSetting camera;

    El3DecOpMode m_opmode;
    const unsigned char *m_origbuf;
    const size_t m_origlen;

    /* this variable functions as a read counter/offset (how deep we are into m_origbuf) */
    size_t m_readxfer;
};
