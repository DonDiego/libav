/*
 * AC-3 parser prototypes
 * Copyright (c) 2003 Fabrice Bellard
 * Copyright (c) 2003 Michael Niedermayer
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_AC3_PARSER_H
#define AVCODEC_AC3_PARSER_H

#include <stdint.h>

#include "ac3.h"

/**
 * Parse AC-3 frame header.
 * Parse the header up to the lfeon element, which is the first 52 or 54 bits
 * depending on the audio coding mode.
 * @param[in]  buffer  buffer containing the first 54 bits of the frame.
 * @param[in]  buffer_size size of the buffer
 * @param[out] hdr Pointer to struct where header info is written.
 * @return Returns number of bits consumed or an AAC_AC3 parser
 * error code on failure.
 */
int avpriv_ac3_parse_header(const uint8_t *buffer, size_t buffer_size, AC3HeaderInfo *hdr);

#endif /* AVCODEC_AC3_PARSER_H */
