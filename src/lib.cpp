/*
 * Copyright (c) 2022 Subreption LLC. All rights reserved.
 * Author: 12dc8242df1be0d6f4b73d68166552197936a27074c004c8af95b27194ec584d
 *
 * Dual-licensed under the Subreption Ukraine Defense License (SUDL, version 1) and the  Server Side
 * Public License (SSPL, version 3). Both licenses are provided with this software distribution.
 */

#include <el3dec/lib.hpp>
#include <el3dec/telemetry.hpp>
#include <cstdlib>
#include <cstddef>

El3Telemetry *el3Decode(const unsigned char *payload, const size_t len, El3DecOpMode mode) 
{
    El3Telemetry *tele = new El3Telemetry(payload, len, mode);

    return tele;
}

