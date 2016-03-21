/*
 * Apple MJPEG-B decoder
 * Copyright (c) 2002 Alex Beregszaszi
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

/**
 * @file
 * Apple MJPEG-B decoder.
 */

#include <inttypes.h>

#include "avcodec.h"
#include "internal.h"
#include "mjpeg.h"
#include "mjpegdec.h"

static uint32_t read_offs(AVCodecContext *avctx, BitstreamContext *bc,
                          uint32_t size, const char *err_msg)
{
    uint32_t offs = bitstream_read(bc, 32);
    if(offs >= size){
        av_log(avctx, AV_LOG_WARNING, err_msg, offs, size);
        return 0;
    }
    return offs;
}

static int mjpegb_decode_frame(AVCodecContext *avctx,
                              void *data, int *got_frame,
                              AVPacket *avpkt)
{
    const uint8_t *buf = avpkt->data;
    int buf_size = avpkt->size;
    MJpegDecodeContext *s = avctx->priv_data;
    const uint8_t *buf_end, *buf_ptr;
    BitstreamContext hbc; /* for the header */
    uint32_t dqt_offs, dht_offs, sof_offs, sos_offs, second_field_offs;
    uint32_t field_size, sod_offs;
    int ret;

    buf_ptr = buf;
    buf_end = buf + buf_size;

read_header:
    /* reset on every SOI */
    s->restart_interval = 0;
    s->restart_count = 0;
    s->mjpb_skiptosod = 0;

    if (buf_end - buf_ptr >= 1 << 28)
        return AVERROR_INVALIDDATA;

    bitstream_init8(&hbc, buf_ptr, /* buf_size */ buf_end - buf_ptr);

    bitstream_skip(&hbc, 32); /* reserved zeros */

    if (bitstream_read(&hbc, 32) != MKBETAG('m','j','p','g')) {
        av_log(avctx, AV_LOG_WARNING, "not mjpeg-b (bad fourcc)\n");
        return AVERROR_INVALIDDATA;
    }

    field_size = bitstream_read(&hbc, 32); /* field size */
    av_log(avctx, AV_LOG_DEBUG, "field size: 0x%"PRIx32"\n", field_size);
    bitstream_skip(&hbc, 32); /* padded field size */
    second_field_offs = read_offs(avctx, &hbc, buf_end - buf_ptr,
                                  "second_field_offs is %d and size is %d\n");
    av_log(avctx, AV_LOG_DEBUG, "second field offs: 0x%"PRIx32"\n",
           second_field_offs);

    dqt_offs = read_offs(avctx, &hbc, buf_end - buf_ptr, "dqt is %d and size is %d\n");
    av_log(avctx, AV_LOG_DEBUG, "dqt offs: 0x%"PRIx32"\n", dqt_offs);
    if (dqt_offs)
    {
        bitstream_init8(&s->bc, buf_ptr + dqt_offs, buf_end - (buf_ptr + dqt_offs));
        s->start_code = DQT;
        if (ff_mjpeg_decode_dqt(s) < 0 &&
            (avctx->err_recognition & AV_EF_EXPLODE))
          return AVERROR_INVALIDDATA;
    }

    dht_offs = read_offs(avctx, &hbc, buf_end - buf_ptr, "dht is %d and size is %d\n");
    av_log(avctx, AV_LOG_DEBUG, "dht offs: 0x%"PRIx32"\n", dht_offs);
    if (dht_offs)
    {
        bitstream_init8(&s->bc, buf_ptr + dht_offs, buf_end - (buf_ptr + dht_offs));
        s->start_code = DHT;
        ff_mjpeg_decode_dht(s);
    }

    sof_offs = read_offs(avctx, &hbc, buf_end - buf_ptr, "sof is %d and size is %d\n");
    av_log(avctx, AV_LOG_DEBUG, "sof offs: 0x%"PRIx32"\n", sof_offs);
    if (sof_offs)
    {
        bitstream_init8(&s->bc, buf_ptr + sof_offs, buf_end - (buf_ptr + sof_offs));
        s->start_code = SOF0;
        if (ff_mjpeg_decode_sof(s) < 0)
            return -1;
    }

    sos_offs = read_offs(avctx, &hbc, buf_end - buf_ptr, "sos is %d and size is %d\n");
    av_log(avctx, AV_LOG_DEBUG, "sos offs: 0x%"PRIx32"\n", sos_offs);
    sod_offs = read_offs(avctx, &hbc, buf_end - buf_ptr, "sof is %d and size is %d\n");
    av_log(avctx, AV_LOG_DEBUG, "sod offs: 0x%"PRIx32"\n", sod_offs);
    if (sos_offs)
    {
        bitstream_init8(&s->bc, buf_ptr + sos_offs,
                        FFMIN(field_size, buf_end - buf_ptr - sos_offs));
        s->mjpb_skiptosod = sod_offs - sos_offs - bitstream_peek(&s->bc, 16);
        s->start_code = SOS;
        if (ff_mjpeg_decode_sos(s, NULL, NULL) < 0 &&
            (avctx->err_recognition & AV_EF_EXPLODE))
          return AVERROR_INVALIDDATA;
    }

    if (s->interlaced) {
        s->bottom_field ^= 1;
        /* if not bottom field, do not output image yet */
        if (s->bottom_field != s->interlace_polarity && second_field_offs)
        {
            buf_ptr = buf + second_field_offs;
            second_field_offs = 0;
            goto read_header;
            }
    }

    //XXX FIXME factorize, this looks very similar to the EOI code

    if ((ret = av_frame_ref(data, s->picture_ptr)) < 0)
        return ret;
    *got_frame = 1;

    if (!s->lossless && avctx->debug & FF_DEBUG_QP) {
        av_log(avctx, AV_LOG_DEBUG, "QP: %d\n",
               FFMAX3(s->qscale[0], s->qscale[1], s->qscale[2]));
    }

    return buf_size;
}

AVCodec ff_mjpegb_decoder = {
    .name           = "mjpegb",
    .long_name      = NULL_IF_CONFIG_SMALL("Apple MJPEG-B"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_MJPEGB,
    .priv_data_size = sizeof(MJpegDecodeContext),
    .init           = ff_mjpeg_decode_init,
    .close          = ff_mjpeg_decode_end,
    .decode         = mjpegb_decode_frame,
    .capabilities   = AV_CODEC_CAP_DR1,
    .caps_internal  = FF_CODEC_CAP_INIT_THREADSAFE,
};
