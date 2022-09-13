/*
 * Copyright (c) 2022 Subreption LLC. All rights reserved.
 * Author: 12dc8242df1be0d6f4b73d68166552197936a27074c004c8af95b27194ec584d
 *
 * Dual-licensed under the Subreption Ukraine Defense License (SUDL, version 1) and the  Server Side
 * Public License (SSPL, version 3). Both licenses are provided with this software distribution.
 */

#pragma once

#include <cstddef>
#include <el3dec/telemetry.hpp>

/**
 * Decode an Eleron 3 payload into suitable structure containing the telemetry information.
 * 
 * 
 * @param  payload
 * @param  len
 * @param  mode
 */

El3Telemetry *el3Decode(const unsigned char *payload, const size_t len, El3DecOpMode mode); 
