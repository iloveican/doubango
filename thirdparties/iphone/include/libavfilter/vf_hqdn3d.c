/*
 * Copyright (c) 2003 Daniel Moreno <comac AT comac DOT darktech DOT org>
 * Copyright (c) 2010 Baptiste Coudurier
 * Copyright (c) 2012 Loren Merritt
 *
 * This file is part of Libav, ported from MPlayer.
 *
 * Libav is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Libav; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * @file
 * high quality 3d video denoiser, ported from MPlayer
 * libmpcodecs/vf_hqdn3d.c.
 */

#include "libavutil/common.h"
#include "libavutil/pixdesc.h"
#include "libavutil/intreadwrite.h"
#include "avfilter.h"
#include "formats.h"
#include "internal.h"
#include "video.h"

typedef struct {
    int16_t coefs[4][512*16];
    uint16_t *line;
    uint16_t *frame_prev[3];
    int hsub, vsub;
    int depth;
} HQDN3DContext;

#define RIGHTSHIFT(a,b) (((a)+(((1<<(b))-1)>>1))>>(b))
#define LOAD(x) ((depth==8 ? src[x] : AV_RN16A(src+(x)*2)) << (16-depth))
#define STORE(x,val) (depth==8 ? dst[x] = RIGHTSHIFT(val, 16-depth)\
                    : AV_WN16A(dst+(x)*2, RIGHTSHIFT(val, 16-depth)))

static inline uint32_t lowpass(int prev, int cur, int16_t *coef)
{
    int d = (prev-cur)>>4;
    return cur + coef[d];
}

av_always_inline
static void denoise_temporal(uint8_t *src, uint8_t *dst,
                             uint16_t *frame_ant,
                             int w, int h, int sstride, int dstride,
                             int16_t *temporal, int depth)
{
    long x, y;
    uint32_t tmp;

    temporal += 0x1000;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            frame_ant[x] = tmp = lowpass(frame_ant[x], LOAD(x), temporal);
            STORE(x, tmp);
        }
        src += sstride;
        dst += dstride;
        frame_ant += w;
    }
}

av_always_inline
static void denoise_spatial(uint8_t *src, uint8_t *dst,
                            uint16_t *line_ant, uint16_t *frame_ant,
                            int w, int h, int sstride, int dstride,
                            int16_t *spatial, int16_t *temporal, int depth)
{
    long x, y;
    uint32_t pixel_ant;
    uint32_t tmp;

    spatial  += 0x1000;
    temporal += 0x1000;

    /* First line has no top neighbor. Only left one for each tmp and
     * last frame */
    pixel_ant = LOAD(0);
    for (x = 0; x < w; x++) {
        line_ant[x] = tmp = pixel_ant = lowpass(pixel_ant, LOAD(x), spatial);
        frame_ant[x] = tmp = lowpass(frame_ant[x], tmp, temporal);
        STORE(x, tmp);
    }

    for (y = 1; y < h; y++) {
        src += sstride;
        dst += dstride;
        frame_ant += w;
        pixel_ant = LOAD(0);
        for (x = 0; x < w-1; x++) {
            line_ant[x] = tmp = lowpass(line_ant[x], pixel_ant, spatial);
            pixel_ant = lowpass(pixel_ant, LOAD(x+1), spatial);
            frame_ant[x] = tmp = lowpass(frame_ant[x], tmp, temporal);
            STORE(x, tmp);
        }
        line_ant[x] = tmp = lowpass(line_ant[x], pixel_ant, spatial);
        frame_ant[x] = tmp = lowpass(frame_ant[x], tmp, temporal);
        STORE(x, tmp);
    }
}

av_always_inline
static void denoise_depth(uint8_t *src, uint8_t *dst,
                          uint16_t *line_ant, uint16_t **frame_ant_ptr,
                          int w, int h, int sstride, int dstride,
                          int16_t *spatial, int16_t *temporal, int depth)
{
    long x, y;
    uint16_t *frame_ant = *frame_ant_ptr;
    if (!frame_ant) {
        uint8_t *frame_src = src;
        *frame_ant_ptr = frame_ant = av_malloc(w*h*sizeof(uint16_t));
        for (y = 0; y < h; y++, src += sstride, frame_ant += w)
            for (x = 0; x < w; x++)
                frame_ant[x] = LOAD(x);
        src = frame_src;
        frame_ant = *frame_ant_ptr;
    }

    if (spatial[0])
        denoise_spatial(src, dst, line_ant, frame_ant,
                        w, h, sstride, dstride, spatial, temporal, depth);
    else
        denoise_temporal(src, dst, frame_ant,
                         w, h, sstride, dstride, temporal, depth);
}

#define denoise(...) \
    switch (hqdn3d->depth) {\
        case  8: denoise_depth(__VA_ARGS__,  8); break;\
        case  9: denoise_depth(__VA_ARGS__,  9); break;\
        case 10: denoise_depth(__VA_ARGS__, 10); break;\
    }

static void precalc_coefs(int16_t *ct, double dist25)
{
    int i;
    double gamma, simil, C;

    gamma = log(0.25) / log(1.0 - FFMIN(dist25,252.0)/255.0 - 0.00001);

    for (i = -255*16; i <= 255*16; i++) {
        // lowpass() truncates (not rounds) the diff, so +15/32 for the midpoint of the bin.
        double f = (i + 15.0/32.0) / 16.0;
        simil = 1.0 - FFABS(f) / 255.0;
        C = pow(simil, gamma) * 256.0 * f;
        ct[16*256+i] = lrint(C);
    }

    ct[0] = !!dist25;
}

#define PARAM1_DEFAULT 4.0
#define PARAM2_DEFAULT 3.0
#define PARAM3_DEFAULT 6.0

static int init(AVFilterContext *ctx, const char *args)
{
    HQDN3DContext *hqdn3d = ctx->priv;
    double lum_spac, lum_tmp, chrom_spac, chrom_tmp;
    double param1, param2, param3, param4;

    lum_spac   = PARAM1_DEFAULT;
    chrom_spac = PARAM2_DEFAULT;
    lum_tmp    = PARAM3_DEFAULT;
    chrom_tmp  = lum_tmp * chrom_spac / lum_spac;

    if (args) {
        switch (sscanf(args, "%lf:%lf:%lf:%lf",
                       &param1, &param2, &param3, &param4)) {
        case 1:
            lum_spac   = param1;
            chrom_spac = PARAM2_DEFAULT * param1 / PARAM1_DEFAULT;
            lum_tmp    = PARAM3_DEFAULT * param1 / PARAM1_DEFAULT;
            chrom_tmp  = lum_tmp * chrom_spac / lum_spac;
            break;
        case 2:
            lum_spac   = param1;
            chrom_spac = param2;
            lum_tmp    = PARAM3_DEFAULT * param1 / PARAM1_DEFAULT;
            chrom_tmp  = lum_tmp * chrom_spac / lum_spac;
            break;
        case 3:
            lum_spac   = param1;
            chrom_spac = param2;
            lum_tmp    = param3;
            chrom_tmp  = lum_tmp * chrom_spac / lum_spac;
            break;
        case 4:
            lum_spac   = param1;
            chrom_spac = param2;
            lum_tmp    = param3;
            chrom_tmp  = param4;
            break;
        }
    }

    av_log(ctx, AV_LOG_VERBOSE, "ls:%lf cs:%lf lt:%lf ct:%lf\n",
           lum_spac, chrom_spac, lum_tmp, chrom_tmp);
    if (lum_spac < 0 || chrom_spac < 0 || isnan(chrom_tmp)) {
        av_log(ctx, AV_LOG_ERROR,
               "Invalid negative value for luma or chroma spatial strength, "
               "or resulting value for chroma temporal strength is nan.\n");
        return AVERROR(EINVAL);
    }

    precalc_coefs(hqdn3d->coefs[0], lum_spac);
    precalc_coefs(hqdn3d->coefs[1], lum_tmp);
    precalc_coefs(hqdn3d->coefs[2], chrom_spac);
    precalc_coefs(hqdn3d->coefs[3], chrom_tmp);

    return 0;
}

static void uninit(AVFilterContext *ctx)
{
    HQDN3DContext *hqdn3d = ctx->priv;

    av_freep(&hqdn3d->line);
    av_freep(&hqdn3d->frame_prev[0]);
    av_freep(&hqdn3d->frame_prev[1]);
    av_freep(&hqdn3d->frame_prev[2]);
}

static int query_formats(AVFilterContext *ctx)
{
    static const enum PixelFormat pix_fmts[] = {
        PIX_FMT_YUV420P,
        PIX_FMT_YUV422P,
        PIX_FMT_YUV444P,
        PIX_FMT_YUV410P,
        PIX_FMT_YUV411P,
        PIX_FMT_YUV440P,
        PIX_FMT_YUVJ420P,
        PIX_FMT_YUVJ422P,
        PIX_FMT_YUVJ444P,
        PIX_FMT_YUVJ440P,
        AV_NE( PIX_FMT_YUV420P9BE, PIX_FMT_YUV420P9LE ),
        AV_NE( PIX_FMT_YUV422P9BE, PIX_FMT_YUV422P9LE ),
        AV_NE( PIX_FMT_YUV444P9BE, PIX_FMT_YUV444P9LE ),
        AV_NE( PIX_FMT_YUV420P10BE, PIX_FMT_YUV420P10LE ),
        AV_NE( PIX_FMT_YUV422P10BE, PIX_FMT_YUV422P10LE ),
        AV_NE( PIX_FMT_YUV444P10BE, PIX_FMT_YUV444P10LE ),
        PIX_FMT_NONE
    };

    ff_set_common_formats(ctx, ff_make_format_list(pix_fmts));

    return 0;
}

static int config_input(AVFilterLink *inlink)
{
    HQDN3DContext *hqdn3d = inlink->dst->priv;

    hqdn3d->hsub = av_pix_fmt_descriptors[inlink->format].log2_chroma_w;
    hqdn3d->vsub = av_pix_fmt_descriptors[inlink->format].log2_chroma_h;
    hqdn3d->depth = av_pix_fmt_descriptors[inlink->format].comp[0].depth_minus1+1;

    hqdn3d->line = av_malloc(inlink->w * sizeof(*hqdn3d->line));
    if (!hqdn3d->line)
        return AVERROR(ENOMEM);

    return 0;
}

static int null_draw_slice(AVFilterLink *link, int y, int h, int slice_dir)
{
    return 0;
}

static int end_frame(AVFilterLink *inlink)
{
    HQDN3DContext *hqdn3d = inlink->dst->priv;
    AVFilterLink *outlink = inlink->dst->outputs[0];
    AVFilterBufferRef *inpic  = inlink ->cur_buf;
    AVFilterBufferRef *outpic = outlink->out_buf;
    int ret, c;

    for (c = 0; c < 3; c++) {
        denoise(inpic->data[c], outpic->data[c],
                hqdn3d->line, &hqdn3d->frame_prev[c],
                inpic->video->w >> (!!c * hqdn3d->hsub),
                inpic->video->h >> (!!c * hqdn3d->vsub),
                inpic->linesize[c], outpic->linesize[c],
                hqdn3d->coefs[c?2:0], hqdn3d->coefs[c?3:1]);
    }

    if ((ret = ff_draw_slice(outlink, 0, inpic->video->h, 1)) < 0 ||
        (ret = ff_end_frame(outlink)) < 0)
        return ret;
    return 0;
}

AVFilter avfilter_vf_hqdn3d = {
    .name          = "hqdn3d",
    .description   = NULL_IF_CONFIG_SMALL("Apply a High Quality 3D Denoiser."),

    .priv_size     = sizeof(HQDN3DContext),
    .init          = init,
    .uninit        = uninit,
    .query_formats = query_formats,

    .inputs    = (const AVFilterPad[]) {{ .name             = "default",
                                          .type             = AVMEDIA_TYPE_VIDEO,
                                          .start_frame      = ff_inplace_start_frame,
                                          .draw_slice       = null_draw_slice,
                                          .config_props     = config_input,
                                          .end_frame        = end_frame },
                                        { .name = NULL}},

    .outputs   = (const AVFilterPad[]) {{ .name             = "default",
                                          .type             = AVMEDIA_TYPE_VIDEO },
                                        { .name = NULL}},
};
