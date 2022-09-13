/*
 * Copyright (c) 2022 Subreption LLC. All rights reserved.
 * Author: 12dc8242df1be0d6f4b73d68166552197936a27074c004c8af95b27194ec584d
 *
 * Dual-licensed under the Subreption Ukraine Defense License (SUDL, version 1) and the  Server Side
 * Public License (SSPL, version 3). Both licenses are provided with this software distribution.
 */

#include <el3dec/lib.hpp>
#include <el3dec/telemetry.hpp>
#include <el3dec/utils.hpp>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include <cstdlib> 
#include <cstdio>
#include <arpa/inet.h>
#include <cmath>
#include <stdexcept>

using namespace std;

const char *exc_invalid_packet_length = "invalid packet data length";

El3Telemetry::El3Telemetry(const unsigned char *buf, const size_t len, El3DecOpMode mode):
    m_origbuf(buf), m_origlen(len), m_opmode(mode)
{
    m_readxfer       = 0;
    
    dataLength       = 0;
    packetType       = 0;
    magicByte        = 0;
    engineType       = 0;
    uavType          = 0;
    uavNo            = 0;
    flightTime       = 0;
    stampHours       = 0;
    stampMinutes     = 0;
    stampSeconds     = 0;
    groundSpeed      = 0.0;
    careen           = 0.0;
    pitch            = 0.0;

    remainingMinutes = 0;
    videoTxChannel   = 0;

    videoTxFreq      = 0;

    memset(&camera, 0, sizeof(struct El3CameraSetting));
    memset(&gpsData, 0, sizeof(struct GpsLocation));

    parseRaw();
}

bool El3Telemetry::checkReadBufferSanity(size_t offset, size_t toread)
{
    if (m_origlen < offset + toread)
    {
        if (m_opmode == FAULT_INTOLERANT)
            throw invalid_argument(exc_invalid_packet_length);

        return false;
    }

    return true;
}

/*
 * Length verifications:
 * Could have used a macro or helper but in order to avoid obscuring the process and making this a
 * reference implementation, every length check before reading is done explicitly.
 * The in-packet length is not trusted or used in any fashion.
 */
void El3Telemetry::parseRaw()
{
    uint8_t typeval = 0;

    /* Verify reduced header is available */
    if (m_origlen < 3)
        throw invalid_argument("invalid packet (no header)");

    m_readxfer += get_byte_from_buf(m_origbuf, 0, &magicByte);

    if (magicByte != ENICS_ELERON_PACKET_MAGICBYTE)
        throw invalid_argument("invalid packet (magic byte missing)");

    m_readxfer += get_byte_from_buf(m_origbuf, 1, &dataLength);

    /* we do not put any implicit trust in the packet length field, but check sanity */
    if (!dataLength || dataLength > m_origlen)
        throw invalid_argument(exc_invalid_packet_length);

    m_readxfer += get_byte_from_buf(m_origbuf, 2, &typeval);

    engineType  = typeval >> 5;
    uavType     = typeval & 0x1F;

    /* verify we can read into the base header */
    if (m_origlen - m_readxfer < sizeof(uint16_t) + sizeof(uint8_t))
        throw invalid_argument(exc_invalid_packet_length);

    /* UAV ID number (16-bit) */
    m_readxfer += get_be_u16_from_buf(m_origbuf, 3, &uavNo);

    /* packet type */
    m_readxfer += get_byte_from_buf(m_origbuf, 5, &packetType);

    /* We are handling a telemetry packet: unpack the data */
    if (packetType == ENICS_ELERON_PACKET_TELEMETRY)
    {
        parseTimestamp();
        parseFlightData();
        parseVideoParams();
    }

    parsingDone();
}

void El3Telemetry::parseTimestamp()
{
    /* timestamp occupies 3 consecutive bytes */
    if (m_origlen - m_readxfer < sizeof(uint8_t) * 3)
        throw invalid_argument(exc_invalid_packet_length);

    /* stamp is UTC */
    m_readxfer += get_byte_from_buf(m_origbuf, 6, &stampHours);
    m_readxfer += get_byte_from_buf(m_origbuf, 7, &stampMinutes);
    m_readxfer += get_byte_from_buf(m_origbuf, 8, &stampSeconds);

    stampHours &= 31;
    stampMinutes &= 61;
    stampSeconds &= 61;

    /* flight time is another uint16_t in big-endian */
    if (checkReadBufferSanity(9, sizeof(uint16_t)))
        m_readxfer += get_be_u16_from_buf(m_origbuf, 9, &flightTime);
}

float El3Telemetry::getPackedCoordinate(size_t off, size_t msboff, int bitshift)
{
    float retval;
    unsigned int coord_int;

    /* extract the MSB for the given coordinate */
    coord_int = ((m_origbuf[msboff] >> bitshift) & 0x7);
    if (coord_int & 4)
        coord_int |= -8;

    coord_int <<= 24;
    coord_int |= (m_origbuf[off] << 16) + (m_origbuf[off+1] << 8) + m_origbuf[off+2];
    retval = (float) coord_int / 6e5;

    /* we dont register the MSB byte yet, do it in the caller when done */
    m_readxfer += 3;

    return retval;
}

void El3Telemetry::parseFlightData()
{
    int tmpint;

    /* coordinates are packed with MSB in one single integer, floats with LSB in their own ints */
    if (m_origlen - m_readxfer < (sizeof(uint32_t) * 2) - 1)
        throw invalid_argument(exc_invalid_packet_length);

    gpsData.latitude    = getPackedCoordinate(0xb, 0xe, 5);
    gpsData.longitude   = getPackedCoordinate(0xf, 0xe, 0);

    /* account for MSB byte for the gps coordinates */
    m_readxfer += 1;

    /* verify altitude field is present */
    if (m_origlen - m_readxfer < sizeof(uint16_t))
        throw invalid_argument(exc_invalid_packet_length);

    /* unpack altitude in meters */
    gpsData.altitude = ((m_origbuf[0x1f] << 8) + m_origbuf[0x20]);
    if (gpsData.altitude >= std::pow(2, 15))
        gpsData.altitude -= std::pow(2, 16);

    m_readxfer += 2;

    /* We are ignoring some fields, in-between.
     * Therefore, maximize the amount of unpacked data by checking we can read far enough into
     * the buffer. Offsets suffice for that.
     */
    if (checkReadBufferSanity(0x12, 0x20 - 0x12))
    {
        
        /* similar to how coordinates are handled */
        groundSpeed = (m_origbuf[0x12] + ((m_origbuf[0x13] & 0xf0) << 4)) * 0.25;
        careen = (((m_origbuf[0x13] & 0x0f) << 8) + m_origbuf[0x14]) * 0.25;
        m_readxfer += 3;

        /* unpack the pitch */
        tmpint = (m_origbuf[0x19] >> 4);
        if (tmpint & 8)
            tmpint |= -0x10;

        tmpint <<= 8;
        tmpint |= m_origbuf[0x18];
        pitch = ((float) tmpint) / 10.0;
        m_readxfer += 2;
    }

    if (checkReadBufferSanity(0x32, sizeof(uint16_t)))
        m_readxfer += get_u16_from_buf(m_origbuf, 0x32, &remainingMinutes);
}

void El3Telemetry::parseVideoParams()
{
    int tmpint;

    /* Frequency should stay within the DVB-T transmitter's capabilities:
     * - The bandpass filter (Mini Circuits CSBP-1228) limits operations to 1203-1253MHz.
     * - Maximum ceiling with DTC D681 downcoverter is 1-1.5GHz.
     */
    if (checkReadBufferSanity(0x44, sizeof(uint8_t)))
    {
        videoTxChannel = m_origbuf[0x44] & 0x0f;
        videoTxFreq = 1205 + videoTxChannel * 3;

        /* Verify frequency is within spec */
        if (videoTxFreq < 1205 || videoTxFreq > 1248)
        {
            if (m_opmode == FAULT_INTOLERANT)
                throw invalid_argument("video tx frequency out of spec, bogus data?");
        }

    }

    /* Read-through to the end of the expected position of camera state information */
    if (m_origlen > 0x50 && checkReadBufferSanity(0x3d, 19))
    {
        camera.angle = (((m_origbuf[0x3e] & 0xe0) << 3) + m_origbuf[0x3d]) / 20.0;

        tmpint = m_origbuf[0x4f] & 0x0f;
        if (tmpint & 8)
            tmpint |= -0x10;

        tmpint <<= 8;
        tmpint |= m_origbuf[0x50];
        camera.position = (float) tmpint / 10.0;

        /* azimuth unpacking */
        tmpint = m_origbuf[0x4f] & 0xf0;
        if (tmpint & 0x80)
            tmpint |= -0x100;

        tmpint <<= 4;
        tmpint |= m_origbuf[0x4e];
        camera.azimuth = (float) tmpint / 10.0;
    }
}

void El3Telemetry::parsingDone()
{
#ifdef DEBUG
    printf(
        "PACKET: TYPE=%d ENGINE=%d UAV_TYPE=%d ID=%d LAT/LON=%f/%f (ALT=%u) SPEED=%f TIME=%d:%d:%d\n"
        "  Flight time: %d\n"
        "  Careen: %f\n"
        "  Minutes remaining: %d\n"
        "  Video: channel %d, freq=%uMHz\n",
        packetType, engineType, uavType, uavNo,
        gpsData.latitude, gpsData.longitude, gpsData.altitude, groundSpeed,
        stampHours, stampMinutes, stampSeconds,
        flightTime,
        careen,
        remainingMinutes,
        videoTxChannel, videoTxFreq
        );
#endif

    /* Place your callbacks here if needed */
}

std::string El3Telemetry::toJson(bool pretty)
{
    rapidjson::Document d;
    d.SetObject();

    rapidjson::Document::AllocatorType& allocator = d.GetAllocator();

    size_t sz = allocator.Size();

    d.AddMember("el3dec_version",   EL3DEC_VERSION, allocator);
    d.AddMember("packet_type",      packetType,     allocator);
    d.AddMember("engine_type",      engineType,     allocator);
    d.AddMember("uav_type",         uavType,        allocator);
    d.AddMember("uav_id",           uavNo,          allocator);

    if (flightTime)
        d.AddMember("flight_time", flightTime, allocator);

    if (remainingMinutes)
        d.AddMember("remaining_min", remainingMinutes, allocator);

    if (careen)
        d.AddMember("careen", careen, allocator);

    if (pitch)
        d.AddMember("pitch", pitch, allocator);

    if (stampHours && stampMinutes && stampSeconds)
    {
        rapidjson::Value time_obj(rapidjson::kObjectType);

        time_obj.AddMember("hours",      stampHours,     allocator);
        time_obj.AddMember("minutes",    stampMinutes,   allocator);
        time_obj.AddMember("seconds",    stampSeconds,   allocator);

        d.AddMember("timestamp", time_obj, allocator);
    }

    if (gpsData.latitude && gpsData.longitude && gpsData.altitude)
    {
        rapidjson::Value gps_obj(rapidjson::kObjectType);

        gps_obj.AddMember("latitude",   gpsData.latitude,   allocator);
        gps_obj.AddMember("longitude",  gpsData.longitude,  allocator);
        gps_obj.AddMember("altitude",   gpsData.altitude,   allocator);
        gps_obj.AddMember("speed",      groundSpeed,        allocator);

        d.AddMember("gps", gps_obj, allocator);
    }

    if (videoTxChannel && videoTxFreq)
    {
        rapidjson::Value video_obj(rapidjson::kObjectType);

        video_obj.AddMember("tx_freq",  videoTxFreq,        allocator);
        video_obj.AddMember("tx_chan",  videoTxChannel,     allocator);

        d.AddMember("video", video_obj, allocator);
    }

    if (camera.angle && camera.azimuth  && camera.position)
    {
        rapidjson::Value cam_obj(rapidjson::kObjectType);

        cam_obj.AddMember("angle",    camera.angle,       allocator);
        cam_obj.AddMember("azimuth",  camera.azimuth,     allocator);
        cam_obj.AddMember("pos",      camera.position,    allocator);

        d.AddMember("camera", cam_obj, allocator);
    }

    rapidjson::StringBuffer strbuf;

    if (!pretty) {
        rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
        d.Accept(writer);
    } else {
         rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(strbuf);
        d.Accept(writer);
    }

    return strbuf.GetString();
}

El3Telemetry::~El3Telemetry() {

}

