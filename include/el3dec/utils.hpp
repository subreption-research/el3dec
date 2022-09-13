/*
 * Copyright (c) 2022 Subreption LLC. All rights reserved.
 * Author: 12dc8242df1be0d6f4b73d68166552197936a27074c004c8af95b27194ec584d
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <memory>

/* This function by iFreilicht: https://creativecommons.org/publicdomain/zero/1.0/ */
template<typename ... Args>
std::string string_format( const std::string& format, Args ... args )
{
    int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1;
    if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
    auto size = static_cast<size_t>( size_s );
    std::unique_ptr<char[]> buf( new char[ size ] );
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 );
}

static inline size_t get_byte_from_buf(const unsigned char *buf, size_t offset, uint8_t *out)
{
    uint8_t *byteval;

    byteval = (uint8_t *) (buf + offset);
    *out = *byteval;

    return sizeof(uint8_t);
}

size_t get_be_u16_from_buf(const unsigned char *buf, size_t offset, uint16_t *out);
size_t get_u16_from_buf(const unsigned char *buf, size_t offset, uint16_t *out);
