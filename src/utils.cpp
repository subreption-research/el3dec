/*
 * Copyright (c) 2022 Subreption LLC. All rights reserved.
 * Author: 12dc8242df1be0d6f4b73d68166552197936a27074c004c8af95b27194ec584d
 *
 * Dual-licensed under the Subreption Ukraine Defense License (SUDL, version 1) and the  Server Side
 * Public License (SSPL, version 3). Both licenses are provided with this software distribution.
 */

#include <cstdlib> 
#include <cstdio>
#include <arpa/inet.h>
#include <el3dec/utils.hpp>

size_t get_be_u16_from_buf(const unsigned char *buf, size_t offset, uint16_t *out)
{
    unsigned short *u16tmp;

    u16tmp = (unsigned short *) (buf + offset);
    *out = htons(*u16tmp);

    return sizeof(unsigned short);
}

size_t get_u16_from_buf(const unsigned char *buf, size_t offset, uint16_t *out)
{
    *out = *((uint16_t *) (buf + offset));

    return sizeof(unsigned short);
}
