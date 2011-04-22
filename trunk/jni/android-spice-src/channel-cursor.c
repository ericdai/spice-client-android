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
#include "spice-channel-cache.h"
#include "spice-marshal.h"

/**
 * SECTION:channel-cursor
 * @short_description: update cursor shape and position
 * @title: Cursor Channel
 * @section_id:
 * @see_also: #SpiceChannel, and the GTK widget #SpiceDisplay
 * @stability: Stable
 * @include: channel-cursor.h
 *
 * The Spice protocol defines a set of messages for controlling cursor
 * shape and position on the remote display area. The cursor changes
 * that should be reflected on the display are notified by
 * signals. See for example #SpiceCursorChannel::cursor-set
 * #SpiceCursorChannel::cursor-move signals.
 */

#define SPICE_CURSOR_CHANNEL_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_CURSOR_CHANNEL, spice_cursor_channel))

typedef struct display_cursor display_cursor;

struct display_cursor {
    SpiceCursorHeader           hdr;
    gboolean                    default_cursor;
    guint32                     data[];
};

struct spice_cursor_channel {
    display_cache               cursors;
    gboolean                    init_done;
};

G_DEFINE_TYPE(SpiceCursorChannel, spice_cursor_channel, SPICE_TYPE_CHANNEL)

enum {
    SPICE_CURSOR_SET,
    SPICE_CURSOR_MOVE,
    SPICE_CURSOR_HIDE,
    SPICE_CURSOR_RESET,

    SPICE_CURSOR_LAST_SIGNAL,
};

static guint signals[SPICE_CURSOR_LAST_SIGNAL];

static void spice_cursor_handle_msg(SpiceChannel *channel, spice_msg_in *msg);
static void delete_cursor_all(SpiceChannel *channel);

/* ------------------------------------------------------------------ */

static void spice_cursor_channel_init(SpiceCursorChannel *channel)
{
    spice_cursor_channel *c;

    c = channel->priv = SPICE_CURSOR_CHANNEL_GET_PRIVATE(channel);
    memset(c, 0, sizeof(*c));

    cache_init(&c->cursors, "cursor");
}

static void spice_cursor_channel_finalize(GObject *obj)
{
    delete_cursor_all(SPICE_CHANNEL(obj));

    if (G_OBJECT_CLASS(spice_cursor_channel_parent_class)->finalize)
        G_OBJECT_CLASS(spice_cursor_channel_parent_class)->finalize(obj);
}

static void spice_cursor_channel_class_init(SpiceCursorChannelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    SpiceChannelClass *channel_class = SPICE_CHANNEL_CLASS(klass);

    gobject_class->finalize     = spice_cursor_channel_finalize;
    channel_class->handle_msg   = spice_cursor_handle_msg;

    /**
     * SpiceCursorChannel::cursor-set:
     * @cursor: the #SpiceCursorChannel that emitted the signal
     * @width: width of the shape
     * @height: height of the shape
     * @hot_x: horizontal offset of the 'hotspot' of the cursor
     * @hot_y: vertical offset of the 'hotspot' of the cursor
     * @rgba: 32bits shape data, or %NULL if default cursor
     *
     * The #SpiceCursorChannel::cursor-set signal is emitted to modify
     * cursor aspect and position on the display area.
     **/
    signals[SPICE_CURSOR_SET] =
        g_signal_new("cursor-set",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceCursorChannelClass, cursor_set),
                     NULL, NULL,
                     g_cclosure_user_marshal_VOID__INT_INT_INT_INT_POINTER,
                     G_TYPE_NONE,
                     5,
                     G_TYPE_INT, G_TYPE_INT,
                     G_TYPE_INT, G_TYPE_INT,
                     G_TYPE_POINTER);

    /**
     * SpiceCursorChannel::cursor-move:
     * @cursor: the #SpiceCursorChannel that emitted the signal
     * @x: x position
     * @y: y position
     *
     * The #SpiceCursorChannel::cursor-move signal is emitted to update
     * the cursor position on the display area.
     **/
    signals[SPICE_CURSOR_MOVE] =
        g_signal_new("cursor-move",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceCursorChannelClass, cursor_move),
                     NULL, NULL,
                     g_cclosure_user_marshal_VOID__INT_INT,
                     G_TYPE_NONE,
                     2,
                     G_TYPE_INT, G_TYPE_INT);

    /**
     * SpiceCursorChannel::cursor-hide:
     * @cursor: the #SpiceCursorChannel that emitted the signal
     *
     * The #SpiceCursorChannel::cursor-hide signal is emitted to hide
     * the cursor/pointer on the display area.
     **/
    signals[SPICE_CURSOR_HIDE] =
        g_signal_new("cursor-hide",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceCursorChannelClass, cursor_hide),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    /**
     * SpiceCursorChannel::cursor-reset:
     * @cursor: the #SpiceCursorChannel that emitted the signal
     *
     * The #SpiceCursorChannel::cursor-reset signal is emitted to
     * reset the cursor to its default context.
     **/
    signals[SPICE_CURSOR_RESET] =
        g_signal_new("cursor-reset",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceCursorChannelClass, cursor_reset),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    g_type_class_add_private(klass, sizeof(spice_cursor_channel));
}

/* signal trampoline---------------------------------------------------------- */

struct SPICE_CURSOR_HIDE {
};

struct SPICE_CURSOR_RESET {
};

struct SPICE_CURSOR_SET {
    uint16_t width;
    uint16_t height;
    uint16_t hot_spot_x;
    uint16_t hot_spot_y;
    gpointer rgba;
};

struct SPICE_CURSOR_MOVE {
    gint x;
    gint y;
};

/* main context */
static void do_emit_main_context(GObject *object, int signum, gpointer params)
{
    switch (signum) {
    case SPICE_CURSOR_HIDE:
    case SPICE_CURSOR_RESET: {
        g_signal_emit(object, signals[signum], 0);
        break;
    }
    case SPICE_CURSOR_SET: {
        struct SPICE_CURSOR_SET *p = params;
        g_signal_emit(object, signals[signum], 0,
                      p->width, p->height, p->hot_spot_x, p->hot_spot_y, p->rgba);
        break;
    }
    case SPICE_CURSOR_MOVE: {
        struct SPICE_CURSOR_MOVE *p = params;
        g_signal_emit(object, signals[signum], 0, p->x, p->y);
        break;
    }
    default:
        g_warn_if_reached();
    }
}

/* ------------------------------------------------------------------ */

static void mono_cursor(display_cursor *cursor, const guint8 *data)
{
    const guint8 *xor, *and;
    guint8 *dest;
    int bpl, x, y, bit;

    bpl = (cursor->hdr.width + 7) / 8;
    and = data;
    xor = and + bpl * cursor->hdr.height;
    dest  = (uint8_t *)cursor->data;
    for (y = 0; y < cursor->hdr.height; y++) {
        bit = 0x80;
        for (x = 0; x < cursor->hdr.width; x++, dest += 4) {
            if (and[x/8] & bit) {
                if (xor[x/8] & bit) {
                    /* flip -> hmm? */
                    dest[0] = 0x00;
                    dest[1] = 0x00;
                    dest[2] = 0x00;
                    dest[3] = 0x80;
                } else {
                    /* unchanged -> transparent */
                    dest[0] = 0x00;
                    dest[1] = 0x00;
                    dest[2] = 0x00;
                    dest[3] = 0x00;
                }
            } else {
                if (xor[x/8] & bit) {
                    /* set -> white */
                    dest[0] = 0xff;
                    dest[1] = 0xff;
                    dest[2] = 0xff;
                    dest[3] = 0xff;
                } else {
                    /* clear -> black */
                    dest[0] = 0x00;
                    dest[1] = 0x00;
                    dest[2] = 0x00;
                    dest[3] = 0xff;
                }
            }
            bit >>= 1;
            if (bit == 0) {
                bit = 0x80;
            }
        }
        and += bpl;
        xor += bpl;
    }
}

static guint8 get_pix_mask(const guint8 *data, gint offset, gint pix_index)
{
    return data[offset + (pix_index >> 3)] & (0x80 >> (pix_index % 8));
}

static guint32 get_pix_hack(gint pix_index, gint width)
{
    return (((pix_index % width) ^ (pix_index / width)) & 1) ? 0xc0303030 : 0x30505050;
}

static display_cursor *set_cursor(SpiceChannel *channel, SpiceCursor *scursor)
{
    spice_cursor_channel *c = SPICE_CURSOR_CHANNEL(channel)->priv;
    SpiceCursorHeader *hdr = &scursor->header;
    display_cache_item *item;
    display_cursor *cursor;
    size_t size;
    gint i, pix_mask, pix;
    const guint8* data;
    guint8 *rgba;
    guint8 val;

    SPICE_DEBUG("%s: type %d, %" PRIx64 ", %dx%d, flags %d, size %d",
            __FUNCTION__, hdr->type, hdr->unique, hdr->width, hdr->height,
            scursor->flags, scursor->data_size);

    if (scursor->flags & SPICE_CURSOR_FLAGS_NONE)
        return NULL;

    if (scursor->flags & SPICE_CURSOR_FLAGS_FROM_CACHE) {
        item = cache_find(&c->cursors, hdr->unique);
        if (!item) {
            return NULL;
        }
        return item->ptr;
    }

    g_return_val_if_fail(scursor->data_size != 0, NULL);

    size = 4 * hdr->width * hdr->height;
    cursor = spice_malloc(sizeof(*cursor) + size);
    cursor->hdr = *hdr;
    cursor->default_cursor = FALSE;
    data = scursor->data;

    switch (hdr->type) {
    case SPICE_CURSOR_TYPE_MONO:
        mono_cursor(cursor, data);
        break;
    case SPICE_CURSOR_TYPE_ALPHA:
        memcpy(cursor->data, data, size);
        break;
    case SPICE_CURSOR_TYPE_COLOR32:
        memcpy(cursor->data, data, size);
        for (i = 0; i < hdr->width * hdr->height; i++) {
            pix_mask = get_pix_mask(data, size, i);
            if (pix_mask && *((guint32*)data + i) == 0xffffff) {
                cursor->data[i] = get_pix_hack(i, hdr->width);
            } else {
                cursor->data[i] |= (pix_mask ? 0 : 0xff000000);
            }
        }
        break;
    case SPICE_CURSOR_TYPE_COLOR16:
        for (i = 0; i < hdr->width * hdr->height; i++) {
            pix_mask = get_pix_mask(data, size, i);
            pix = *((guint16*)data + i);
            if (pix_mask && pix == 0x7fff) {
                cursor->data[i] = get_pix_hack(i, hdr->width);
            } else {
                cursor->data[i] |= ((pix & 0x1f) << 3) | ((pix & 0x3e0) << 6) |
                    ((pix & 0x7c00) << 9) | (pix_mask ? 0 : 0xff000000);
            }
        }
        break;
    case SPICE_CURSOR_TYPE_COLOR4:
        size = (SPICE_ALIGN(hdr->width, 2) / 2) * hdr->height;
        for (i = 0; i < hdr->width * hdr->height; i++) {
            pix_mask = get_pix_mask(data, size + (sizeof(uint32_t) << 4), i);
            int idx = (i & 1) ? (data[i >> 1] & 0x0f) : ((data[i >> 1] & 0xf0) >> 4);
            pix = *((uint32_t*)(data + size) + idx);
            if (pix_mask && pix == 0xffffff) {
                cursor->data[i] = get_pix_hack(i, hdr->width);
            } else {
                cursor->data[i] = pix | (pix_mask ? 0 : 0xff000000);
            }
        }

        break;
    default:
        g_warning("%s: unimplemented cursor type %d", __FUNCTION__,
                  hdr->type);
        cursor->default_cursor = TRUE;
        goto cache_add;
    }

    rgba = (guint8*)cursor->data;
    for (i = 0; i < hdr->width * hdr->height; i++) {
        val = rgba[0];
        rgba[0] = rgba[2];
        rgba[2] = val;
        rgba += 4;
    }

cache_add:
    if (cursor && (scursor->flags & SPICE_CURSOR_FLAGS_CACHE_ME)) {
        item = cache_add(&c->cursors, hdr->unique);
        item->ptr = cursor;
    }

    return cursor;
}

static void delete_cursor_one(SpiceChannel *channel, display_cache_item *item)
{
    spice_cursor_channel *c = SPICE_CURSOR_CHANNEL(channel)->priv;

    free(item->ptr);
    cache_del(&c->cursors, item);
}

static void delete_cursor_all(SpiceChannel *channel)
{
    spice_cursor_channel *c = SPICE_CURSOR_CHANNEL(channel)->priv;
    display_cache_item *item;

    for (;;) {
        item = cache_get_lru(&c->cursors);
        if (item == NULL) {
            return;
        }
        delete_cursor_one(channel, item);
    }
}

/* coroutine context */
static void emit_cursor_set(SpiceChannel *channel, display_cursor *cursor)
{
    g_return_if_fail(cursor != NULL);
    emit_main_context(channel, SPICE_CURSOR_SET,
                      cursor->hdr.width, cursor->hdr.height,
                      cursor->hdr.hot_spot_x, cursor->hdr.hot_spot_y,
                      cursor->default_cursor ? NULL : cursor->data);
}

/* coroutine context */
static void cursor_handle_init(SpiceChannel *channel, spice_msg_in *in)
{
    SpiceMsgCursorInit *init = spice_msg_in_parsed(in);
    spice_cursor_channel *c = SPICE_CURSOR_CHANNEL(channel)->priv;
    display_cursor *cursor;

    g_return_if_fail(c->init_done == FALSE);

    delete_cursor_all(channel);
    cursor = set_cursor(channel, &init->cursor);
    c->init_done = TRUE;
    if (init->visible && cursor)
        emit_cursor_set(channel, cursor);
    else
        emit_main_context(channel, SPICE_CURSOR_HIDE);
}

/* coroutine context */
static void cursor_handle_reset(SpiceChannel *channel, spice_msg_in *in)
{
    spice_cursor_channel *c = SPICE_CURSOR_CHANNEL(channel)->priv;

    SPICE_DEBUG("%s, init_done: %d", __FUNCTION__, c->init_done);

    delete_cursor_all(channel);
    emit_main_context(channel, SPICE_CURSOR_RESET);
    c->init_done = FALSE;
}

/* coroutine context */
static void cursor_handle_set(SpiceChannel *channel, spice_msg_in *in)
{
    SpiceMsgCursorSet *set = spice_msg_in_parsed(in);
    spice_cursor_channel *c = SPICE_CURSOR_CHANNEL(channel)->priv;
    display_cursor *cursor;

    g_return_if_fail(c->init_done == TRUE);

    cursor = set_cursor(channel, &set->cursor);
    emit_cursor_set(channel, cursor);
}

/* coroutine context */
static void cursor_handle_move(SpiceChannel *channel, spice_msg_in *in)
{
    SpiceMsgCursorMove *move = spice_msg_in_parsed(in);
    spice_cursor_channel *c = SPICE_CURSOR_CHANNEL(channel)->priv;

    g_return_if_fail(c->init_done == TRUE);

    emit_main_context(channel, SPICE_CURSOR_MOVE,
                      move->position.x, move->position.y);
}

/* coroutine context */
static void cursor_handle_hide(SpiceChannel *channel, spice_msg_in *in)
{
#ifdef EXTRA_CHECKS
    spice_cursor_channel *c = SPICE_CURSOR_CHANNEL(channel)->priv;

    g_return_if_fail(c->init_done == TRUE);
#endif

    emit_main_context(channel, SPICE_CURSOR_HIDE);
}

/* coroutine context */
static void cursor_handle_trail(SpiceChannel *channel, spice_msg_in *in)
{
    spice_cursor_channel *c = SPICE_CURSOR_CHANNEL(channel)->priv;

    g_return_if_fail(c->init_done == TRUE);

    g_warning("%s: TODO", __FUNCTION__);
}

/* coroutine context */
static void cursor_handle_inval_one(SpiceChannel *channel, spice_msg_in *in)
{
    spice_cursor_channel *c = SPICE_CURSOR_CHANNEL(channel)->priv;
    SpiceMsgDisplayInvalOne *zap = spice_msg_in_parsed(in);
    display_cache_item *item;

    g_return_if_fail(c->init_done == TRUE);

    item = cache_find(&c->cursors, zap->id);
    delete_cursor_one(channel, item);
}

/* coroutine context */
static void cursor_handle_inval_all(SpiceChannel *channel, spice_msg_in *in)
{
    delete_cursor_all(channel);
}

static spice_msg_handler cursor_handlers[] = {
    [ SPICE_MSG_SET_ACK ]                  = spice_channel_handle_set_ack,
    [ SPICE_MSG_PING ]                     = spice_channel_handle_ping,
    [ SPICE_MSG_NOTIFY ]                   = spice_channel_handle_notify,
    [ SPICE_MSG_DISCONNECTING ]            = spice_channel_handle_disconnect,
    [ SPICE_MSG_WAIT_FOR_CHANNELS ]        = spice_channel_handle_wait_for_channels,
    [ SPICE_MSG_MIGRATE ]                  = spice_channel_handle_migrate,

    [ SPICE_MSG_CURSOR_INIT ]              = cursor_handle_init,
    [ SPICE_MSG_CURSOR_RESET ]             = cursor_handle_reset,
    [ SPICE_MSG_CURSOR_SET ]               = cursor_handle_set,
    [ SPICE_MSG_CURSOR_MOVE ]              = cursor_handle_move,
    [ SPICE_MSG_CURSOR_HIDE ]              = cursor_handle_hide,
    [ SPICE_MSG_CURSOR_TRAIL ]             = cursor_handle_trail,
    [ SPICE_MSG_CURSOR_INVAL_ONE ]         = cursor_handle_inval_one,
    [ SPICE_MSG_CURSOR_INVAL_ALL ]         = cursor_handle_inval_all,
};

/* coroutine context */
static void spice_cursor_handle_msg(SpiceChannel *channel, spice_msg_in *msg)
{
    int type = spice_msg_in_type(msg);
    g_return_if_fail(type < SPICE_N_ELEMENTS(cursor_handlers));
    g_return_if_fail(cursor_handlers[type] != NULL);
    cursor_handlers[type](channel, msg);
}
