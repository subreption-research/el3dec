#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <el3dec/lib.hpp>
#include <el3dec/telemetry.hpp>
#include <fstream>
#include <string>
#include <iostream>
#include <cstdlib>
#include <functional>
#include <memory>
#include <vector>

#define MAX_PAYLOAD_BYTES 256

static unsigned char payload_ok[] = {
    0xAA, 0x61, 0x21, 0x05, 0x39, 0x0F, 0x10, 0x70, 0x3A, 0x03, 0xCA, 0xB4, 0x6D, 0xA3, 0x31, 0x4E,
    0x1D, 0x2A, 0xDE, 0x01, 0x4B, 0x62, 0xAB, 0xD0, 0x84, 0xBF, 0xFC, 0x02, 0x58, 0x02, 0x57, 0x03,
    0x0D, 0x0C, 0x21, 0x02, 0x04, 0x00, 0x2D, 0x02, 0x04, 0x30, 0x1F, 0x81, 0x10, 0x14, 0x00, 0xCD,
    0x38, 0x52, 0x5A, 0x00, 0x98, 0x06, 0x82, 0x4B, 0x31, 0x26, 0x00, 0x00, 0x01, 0x43, 0x01, 0xE8,
    0x6F, 0x00, 0x02, 0xF9, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0xCF,
    0x77, 0x01, 0x01, 0x08, 0x63, 0x10, 0x03, 0x22, 0x01, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0xCC,
    0x26, 0x00, 0x32, 0xB2
};

// a function to convert a hex string into binary format
std::vector<std::uint8_t> str2bin(std::string_view hash_hex)
{
    static constexpr std::string_view tab = "0123456789abcdef";

    std::vector<std::uint8_t> res(MAX_PAYLOAD_BYTES);

    if (hash_hex.size() < MAX_PAYLOAD_BYTES * 2)
    {
        for(size_t i = 0; i < res.size(); ++i)
        {
            // find the first nibble and left shift it 4 steps, then find the
            // second nibble and do a bitwise OR to combine them:
            res[i] = tab.find(hash_hex[i*2])<<4 | tab.find(hash_hex[i*2+1]);
        }
    }
    return res;
}

void readSamples(std::string fileName, std::vector<std::string>& vec, unsigned int max)
{
    unsigned int cnt = 0;

    std::ifstream file(fileName);

    std::string str;
    while (std::getline(file, str))
    {
        if (max && cnt > max)
            break;

        if (str.size() > 0) {
            vec.push_back(str);
            cnt++;
        }
    }

    file.close();
}

enum { NS_PER_SECOND = 1000000000 };

void sub_timespec(struct timespec t1, struct timespec t2, struct timespec *td)
{
    td->tv_nsec = t2.tv_nsec - t1.tv_nsec;
    td->tv_sec  = t2.tv_sec - t1.tv_sec;
    if (td->tv_sec > 0 && td->tv_nsec < 0)
    {
        td->tv_nsec += NS_PER_SECOND;
        td->tv_sec--;
    }
    else if (td->tv_sec < 0 && td->tv_nsec > 0)
    {
        td->tv_nsec -= NS_PER_SECOND;
        td->tv_sec++;
    }
}

TEST_CASE("el3dec Telemetry Decoding (invalid input)")
{
    El3Telemetry *telemetry;
    static unsigned char payload_corrupted[sizeof(payload_ok)];

    SECTION("Invalid header magic")
    {
        memcpy(payload_corrupted, payload_ok, sizeof(payload_ok));
        payload_corrupted[0] = 0xA0;
        REQUIRE_THROWS_AS(telemetry = el3Decode(payload_corrupted, sizeof(payload_corrupted),
            FAULT_INTOLERANT), std::invalid_argument);
        delete telemetry;
        telemetry = NULL;
    }

    SECTION("Insufficient data")
    {
        unsigned char insufficient[2] = { 0xAA, 1 };
        REQUIRE_THROWS_AS(telemetry = el3Decode(insufficient, sizeof(insufficient),
            FAULT_INTOLERANT), std::invalid_argument);
        delete telemetry;
        telemetry = NULL;
    }

    SECTION("Invalid header data length")
    {
        memcpy(payload_corrupted, payload_ok, sizeof(payload_ok));
        payload_corrupted[1] = -126;
        REQUIRE_THROWS_AS(telemetry = el3Decode(payload_corrupted, sizeof(payload_corrupted),
            FAULT_INTOLERANT), std::invalid_argument);
        delete telemetry;
        telemetry = NULL;
    }

    SECTION("Truncated GPS data")
    {
        memcpy(payload_corrupted, payload_ok, sizeof(payload_ok));
        REQUIRE_THROWS_AS(telemetry = el3Decode(payload_corrupted, 0xe + 1,
            FAULT_INTOLERANT), std::invalid_argument);
        delete telemetry;
        telemetry = NULL;

        REQUIRE_THROWS_AS(telemetry = el3Decode(payload_corrupted, 0xe + 2,
            FAULT_INTOLERANT), std::invalid_argument);
        delete telemetry;
        telemetry = NULL;

        REQUIRE_THROWS_AS(telemetry = el3Decode(payload_corrupted, 0xe + 3,
            FAULT_INTOLERANT), std::invalid_argument);
        delete telemetry;
        telemetry = NULL;
    }

    SECTION("Invalid video frequency (out of spec)")
    {
        memcpy(payload_corrupted, payload_ok, sizeof(payload_ok));
        payload_corrupted[0x44] = 254 | 0x0f;
        REQUIRE_THROWS_AS(telemetry = el3Decode(payload_corrupted, sizeof(payload_corrupted),
            FAULT_INTOLERANT), std::invalid_argument);
        delete telemetry;
        telemetry = NULL;
    }

    if (telemetry != NULL)
        delete telemetry;

    SUCCEED();
}

TEST_CASE("el3dec Telemetry Decoding (single payload, fault intolerant)")
{
    El3Telemetry *telemetry;

    BENCHMARK("el3Decode (single)")
    {
        telemetry = el3Decode(payload_ok, sizeof(payload_ok), FAULT_INTOLERANT);
    };

#if 0
    BENCHMARK("el3Decode (single) JSON output")
    {
        telemetry->toJson(false);
    };
#endif

    REQUIRE(telemetry != NULL);
    REQUIRE(telemetry->ID() == 1337);
    REQUIRE(telemetry->Type() == 1);
    REQUIRE(telemetry->RemainingFlightMinutes() == 90);
    REQUIRE(telemetry->Altitude() == 781);
    REQUIRE(telemetry->Groundspeed() == 55.5);
    REQUIRE(telemetry->VideoFreq() == 1214);
    REQUIRE(telemetry->CameraAngle() == 3.3499999046325684);
    REQUIRE(telemetry->CameraAzimuth() == -88.80000305175781);
    REQUIRE(telemetry->CameraPosition() == -13.699999809265137);
    REQUIRE(telemetry->Latitude() == 47.66960525512695);
    REQUIRE(telemetry->Longitude() == 36.49414825439453);

    delete telemetry;
    SUCCEED();
}

TEST_CASE("el3dec Telemetry Decoding (from samples)")
{
    timespec start, finish, delta;

    std::vector<std::string> vecHexLines;
    readSamples("../tests/fixtures/telemetry-samples.txt", vecHexLines, 0);

    if (!vecHexLines.size())
        FAIL("Failed to load any samples, check file exists!");

    REQUIRE(vecHexLines.size() == 2037);
    
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
    for (auto &s: vecHexLines)
    {
        auto bindata = str2bin(s);

        El3Telemetry *telemetry = el3Decode(bindata.data(), bindata.size(), FAULT_TOLERANT);
        delete telemetry;
    }
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &finish);
    sub_timespec(start, finish, &delta);

    printf("Processing %lu samples took %d.%.9ld seconds\n", vecHexLines.size(), (int)delta.tv_sec,
        delta.tv_nsec);
}
