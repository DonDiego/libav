/*
 * MPEG-4 Audio common code
 * Copyright (c) 2008 Baptiste Coudurier <baptiste.coudurier@free.fr>
 * Copyright (c) 2009 Alex Converse <alex.converse@gmail.com>
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

#include "bitstream.h"
#include "put_bits.h"
#include "mpeg4audio.h"

/**
 * Parse MPEG-4 audio configuration for ALS object type.
 * @param[in] bc       bit reader context
 * @param[in] c        MPEG4AudioConfig structure to fill
 * @return on success 0 is returned, otherwise a value < 0
 */
static int parse_config_ALS(BitstreamContext *bc, MPEG4AudioConfig *c)
{
    if (bitstream_bits_left(bc) < 112)
        return AVERROR_INVALIDDATA;

    if (bitstream_read(bc, 32) != MKBETAG('A','L','S','\0'))
        return AVERROR_INVALIDDATA;

    // override AudioSpecificConfig channel configuration and sample rate
    // which are buggy in old ALS conformance files
    c->sample_rate = bitstream_read(bc, 32);

    // skip number of samples
    bitstream_skip(bc, 32);

    // read number of channels
    c->chan_config = 0;
    c->channels    = bitstream_read(bc, 16) + 1;

    return 0;
}

const int avpriv_mpeg4audio_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};

const uint8_t ff_mpeg4audio_channels[8] = {
    0, 1, 2, 3, 4, 5, 6, 8
};

static inline int get_object_type(BitstreamContext *bc)
{
    int object_type = bitstream_read(bc, 5);
    if (object_type == AOT_ESCAPE)
        object_type = 32 + bitstream_read(bc, 6);
    return object_type;
}

static inline int get_sample_rate(BitstreamContext *bc, int *index)
{
    *index = bitstream_read(bc, 4);
    return *index == 0x0f ? bitstream_read(bc, 24) :
        avpriv_mpeg4audio_sample_rates[*index];
}

int avpriv_mpeg4audio_get_config(MPEG4AudioConfig *c, const uint8_t *buf,
                                 int bit_size, int sync_extension)
{
    BitstreamContext bc;
    int specific_config_bitindex, ret;

    ret = bitstream_init(&bc, buf, bit_size);
    if (ret < 0)
        return ret;
    c->object_type = get_object_type(&bc);
    c->sample_rate = get_sample_rate(&bc, &c->sampling_index);
    c->chan_config = bitstream_read(&bc, 4);
    if (c->chan_config < FF_ARRAY_ELEMS(ff_mpeg4audio_channels))
        c->channels = ff_mpeg4audio_channels[c->chan_config];
    c->sbr = -1;
    c->ps  = -1;
    if (c->object_type == AOT_SBR || (c->object_type == AOT_PS &&
        // check for W6132 Annex YYYY draft MP3onMP4
        !(bitstream_peek(&bc, 3) & 0x03 && !(bitstream_peek(&bc, 9) & 0x3F)))) {
        if (c->object_type == AOT_PS)
            c->ps = 1;
        c->ext_object_type = AOT_SBR;
        c->sbr = 1;
        c->ext_sample_rate = get_sample_rate(&bc, &c->ext_sampling_index);
        c->object_type = get_object_type(&bc);
        if (c->object_type == AOT_ER_BSAC)
            c->ext_chan_config = bitstream_read(&bc, 4);
    } else {
        c->ext_object_type = AOT_NULL;
        c->ext_sample_rate = 0;
    }
    specific_config_bitindex = bitstream_tell(&bc);

    if (c->object_type == AOT_ALS) {
        bitstream_skip(&bc, 5);
        if (bitstream_peek(&bc, 24) != MKBETAG('\0','A','L','S'))
            bitstream_skip(&bc, 24);

        specific_config_bitindex = bitstream_tell(&bc);

        ret = parse_config_ALS(&bc, c);
        if (ret < 0)
            return ret;
    }

    if (c->ext_object_type != AOT_SBR && sync_extension) {
        while (bitstream_bits_left(&bc) > 15) {
            if (bitstream_peek(&bc, 11) == 0x2b7) { // sync extension
                bitstream_read(&bc, 11);
                c->ext_object_type = get_object_type(&bc);
                if (c->ext_object_type == AOT_SBR && (c->sbr = bitstream_read_bit(&bc)) == 1)
                    c->ext_sample_rate = get_sample_rate(&bc, &c->ext_sampling_index);
                if (bitstream_bits_left(&bc) > 11 && bitstream_read(&bc, 11) == 0x548)
                    c->ps = bitstream_read_bit(&bc);
                break;
            } else
                bitstream_read_bit(&bc); // skip 1 bit
        }
    }

    //PS requires SBR
    if (!c->sbr)
        c->ps = 0;
    //Limit implicit PS to the HE-AACv2 Profile
    if ((c->ps == -1 && c->object_type != AOT_AAC_LC) || c->channels & ~0x01)
        c->ps = 0;

    return specific_config_bitindex;
}

static av_always_inline unsigned int copy_bits(PutBitContext *pb,
                                               BitstreamContext *bc,
                                               int bits)
{
    unsigned int el = bitstream_read(bc, bits);
    put_bits(pb, bits, el);
    return el;
}

int avpriv_copy_pce_data(PutBitContext *pb, BitstreamContext *bc)
{
    int five_bit_ch, four_bit_ch, comment_size, bits;
    int offset = put_bits_count(pb);

    copy_bits(pb, bc, 10);                  //Tag, Object Type, Frequency
    five_bit_ch  = copy_bits(pb, bc, 4);    //Front
    five_bit_ch += copy_bits(pb, bc, 4);    //Side
    five_bit_ch += copy_bits(pb, bc, 4);    //Back
    four_bit_ch  = copy_bits(pb, bc, 2);    //LFE
    four_bit_ch += copy_bits(pb, bc, 3);    //Data
    five_bit_ch += copy_bits(pb, bc, 4);    //Coupling
    if (copy_bits(pb, bc, 1))               //Mono Mixdown
        copy_bits(pb, bc, 4);
    if (copy_bits(pb, bc, 1))               //Stereo Mixdown
        copy_bits(pb, bc, 4);
    if (copy_bits(pb, bc, 1))               //Matrix Mixdown
        copy_bits(pb, bc, 3);
    for (bits = five_bit_ch*5+four_bit_ch*4; bits > 16; bits -= 16)
        copy_bits(pb, bc, 16);
    if (bits)
        copy_bits(pb, bc, bits);
    avpriv_align_put_bits(pb);
    bitstream_align(bc);
    comment_size = copy_bits(pb, bc, 8);
    for (; comment_size > 0; comment_size--)
        copy_bits(pb, bc, 8);

    return put_bits_count(pb) - offset;
}
