/*
 * Copyright (c) 2007 Bobby Bingham
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
 * audio and video splitter
 */

#include <stdio.h>

#include "libavutil/internal.h"
#include "libavutil/mem.h"
#include "avfilter.h"
#include "audio.h"
#include "internal.h"
#include "video.h"

static int split_init(AVFilterContext *ctx, const char *args)
{
    int i, nb_outputs = 2;

    if (args) {
        nb_outputs = strtol(args, NULL, 0);
        if (nb_outputs <= 0) {
            av_log(ctx, AV_LOG_ERROR, "Invalid number of outputs specified: %d.\n",
                   nb_outputs);
            return AVERROR(EINVAL);
        }
    }

    for (i = 0; i < nb_outputs; i++) {
        char name[32];
        AVFilterPad pad = { 0 };

        snprintf(name, sizeof(name), "output%d", i);
        pad.type = ctx->filter->inputs[0].type;
        pad.name = av_strdup(name);

        ff_insert_outpad(ctx, i, &pad);
    }

    return 0;
}

static void split_uninit(AVFilterContext *ctx)
{
    int i;

    for (i = 0; i < ctx->nb_outputs; i++)
        av_freep(&ctx->output_pads[i].name);
}

static int filter_frame(AVFilterLink *inlink, AVFrame *frame)
{
    AVFilterContext *ctx = inlink->dst;
    int i, ret = 0;

    for (i = 0; i < ctx->nb_outputs; i++) {
        AVFrame *buf_out = av_frame_clone(frame);
        if (!buf_out) {
            ret = AVERROR(ENOMEM);
            break;
        }

        ret = ff_filter_frame(ctx->outputs[i], buf_out);
        if (ret < 0)
            break;
    }
    av_frame_free(&frame);
    return ret;
}

static const AVFilterPad avfilter_vf_split_inputs[] = {
    {
        .name             = "default",
        .type             = AVMEDIA_TYPE_VIDEO,
        .get_video_buffer = ff_null_get_video_buffer,
        .filter_frame     = filter_frame,
    },
    { NULL }
};

AVFilter avfilter_vf_split = {
    .name      = "split",
    .description = NULL_IF_CONFIG_SMALL("Pass on the input to two outputs."),

    .init   = split_init,
    .uninit = split_uninit,

    .inputs    = avfilter_vf_split_inputs,
    .outputs   = NULL,
};

static const AVFilterPad avfilter_af_asplit_inputs[] = {
    {
        .name             = "default",
        .type             = AVMEDIA_TYPE_AUDIO,
        .get_audio_buffer = ff_null_get_audio_buffer,
        .filter_frame     = filter_frame,
    },
    { NULL }
};

AVFilter avfilter_af_asplit = {
    .name        = "asplit",
    .description = NULL_IF_CONFIG_SMALL("Pass on the audio input to N audio outputs."),

    .init   = split_init,
    .uninit = split_uninit,

    .inputs  = avfilter_af_asplit_inputs,
    .outputs = NULL,
};
