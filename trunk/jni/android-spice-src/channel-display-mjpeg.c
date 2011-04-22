/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#include "spice-client.h"
#include "spice-common.h"
#include "spice-channel-priv.h"

#include "channel-display-priv.h"

static void mjpeg_src_init(struct jpeg_decompress_struct *cinfo)
{
    display_stream *st = SPICE_CONTAINEROF(cinfo->src, display_stream, mjpeg_src);
    SpiceMsgDisplayStreamData *data = spice_msg_in_parsed(st->msg_data);

    cinfo->src->next_input_byte = data->data;
    cinfo->src->bytes_in_buffer = data->data_size;
}

static int mjpeg_src_fill(struct jpeg_decompress_struct *cinfo)
{
    g_critical("need more input data");
    return 0;
}

static void mjpeg_src_skip(struct jpeg_decompress_struct *cinfo,
                           long num_bytes)
{
    cinfo->src->next_input_byte += num_bytes;
}

static void mjpeg_src_term(struct jpeg_decompress_struct *cinfo)
{
    /* nothing */
}

G_GNUC_INTERNAL
void stream_mjpeg_init(display_stream *st)
{
    st->mjpeg_cinfo.err = jpeg_std_error(&st->mjpeg_jerr);
    jpeg_create_decompress(&st->mjpeg_cinfo);

    st->mjpeg_src.init_source         = mjpeg_src_init;
    st->mjpeg_src.fill_input_buffer   = mjpeg_src_fill;
    st->mjpeg_src.skip_input_data     = mjpeg_src_skip;
    st->mjpeg_src.resync_to_restart   = jpeg_resync_to_restart;
    st->mjpeg_src.term_source         = mjpeg_src_term;
    st->mjpeg_cinfo.src               = &st->mjpeg_src;
}

static void mjpeg_convert_scanline(uint8_t *dest, uint8_t *src, int width, int compat)
{
    uint32_t *row = (void*)dest;
    uint32_t c;
    int x;

    if (compat) {
        /*
         * We need to check for the old major and for backwards compat
         *  a) swap r and b (done)
         *  b) to-yuv with right values and then from-yuv with old wrong values (TODO)
         */
        for (x = 0; x < width; x++) {
            c = src[2] << 16 | src[1] << 8 | src[0];
            src += 3;
            *row++ = c;
        }
    } else {
        for (x = 0; x < width; x++) {
            c = src[0] << 16 | src[1] << 8 | src[2];
            src += 3;
            *row++ = c;
        }
    }
}

G_GNUC_INTERNAL
void stream_mjpeg_data(display_stream *st)
{
    SpiceMsgDisplayStreamCreate *info = spice_msg_in_parsed(st->msg_create);
    int width = info->stream_width;
    int height = info->stream_height;
    uint8_t *line, *dest;
    int i;

    line = malloc(width * 3);
    dest = malloc(width * height * 4);

    if (st->out_frame) {
        free(st->out_frame);
    }
    st->out_frame = dest;

    jpeg_read_header(&st->mjpeg_cinfo, 1);
    st->mjpeg_cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&st->mjpeg_cinfo);
    for (i = 0; i < height; i++) {
        jpeg_read_scanlines(&st->mjpeg_cinfo, &line, 1);
        mjpeg_convert_scanline(dest, line, width, 0 /* FIXME: compat */);
        dest += 4 * width;
    }
    jpeg_finish_decompress(&st->mjpeg_cinfo);

    free(line);
}

G_GNUC_INTERNAL
void stream_mjpeg_cleanup(display_stream *st)
{
    jpeg_destroy_decompress(&st->mjpeg_cinfo);
}
