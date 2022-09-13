/*
 * Copyright (c) 2022 Subreption LLC. All rights reserved.
 * Author: 12dc8242df1be0d6f4b73d68166552197936a27074c004c8af95b27194ec584d
 *
 * Dual-licensed under the Subreption Ukraine Defense License (SUDL, version 1) and the  Server Side
 * Public License (SSPL, version 3). Both licenses are provided with this software distribution.
 */

#include <el3dec/lib.hpp>
#include <el3dec/telemetry.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#define MAX_PAYLOAD_BYTES 256

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

int main(int argc, char **argv)
{
    std::vector<std::string> vecHexLines;

    if (argc != 2)
    {
        std::cerr << "Usage: el3dec_app [path]\n";
        return EXIT_FAILURE;
    }

    readSamples(argv[1], vecHexLines, 0);

    for (auto &s: vecHexLines)
    {
        std::cout << "Decoding: " << s << std::endl;

        auto bindata = str2bin(s);

        El3Telemetry *telemetry = el3Decode(bindata.data(), bindata.size(), FAULT_TOLERANT);

        std::cout << "Decoded telemetry:" << std::endl;
        std::string jsonStr = telemetry->toJson(true);
        std::cout << jsonStr << "\n";

        delete telemetry;
    }

    return 0;
}
