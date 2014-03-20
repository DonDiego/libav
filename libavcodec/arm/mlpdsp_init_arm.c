/*
 * Copyright (c) 2014 RISC OS Open Ltd
 * Author: Ben Avison <bavison@riscosopen.org>
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

#include <stdint.h>

#include "libavutil/arm/cpu.h"
#include "libavutil/attributes.h"
#include "libavcodec/mlpdsp.h"

void ff_mlp_filter_channel_arm(int32_t *state, const int32_t *coeff,
                               int firorder, int iirorder,
                               unsigned int filter_shift, int32_t mask,
                               int blocksize, int32_t *sample_buffer);
void ff_mlp_rematrix_channel_arm(int32_t *samples,
                                 const int32_t *coeffs,
                                 const uint8_t *bypassed_lsbs,
                                 const int8_t *noise_buffer,
                                 int index,
                                 unsigned int dest_ch,
                                 uint16_t blockpos,
                                 unsigned int maxchan,
                                 int matrix_noise_shift,
                                 int access_unit_size_pow2,
                                 int32_t mask);

av_cold void ff_mlpdsp_init_arm(MLPDSPContext *c)
{
    int cpu_flags = av_get_cpu_flags();

    if (have_armv5te(cpu_flags)) {
        c->mlp_filter_channel = ff_mlp_filter_channel_arm;
        c->mlp_rematrix_channel = ff_mlp_rematrix_channel_arm;
    }
}