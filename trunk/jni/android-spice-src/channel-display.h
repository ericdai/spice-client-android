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
#ifndef __SPICE_CLIENT_DISPLAY_CHANNEL_H__
#define __SPICE_CLIENT_DISPLAY_CHANNEL_H__

#include "spice-client.h"

G_BEGIN_DECLS

#define SPICE_TYPE_DISPLAY_CHANNEL            (spice_display_channel_get_type())
#define SPICE_DISPLAY_CHANNEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SPICE_TYPE_DISPLAY_CHANNEL, SpiceDisplayChannel))
#define SPICE_DISPLAY_CHANNEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SPICE_TYPE_DISPLAY_CHANNEL, SpiceDisplayChannelClass))
#define SPICE_IS_DISPLAY_CHANNEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPICE_TYPE_DISPLAY_CHANNEL))
#define SPICE_IS_DISPLAY_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPICE_TYPE_DISPLAY_CHANNEL))
#define SPICE_DISPLAY_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), SPICE_TYPE_DISPLAY_CHANNEL, SpiceDisplayChannelClass))

typedef struct _SpiceDisplayChannel SpiceDisplayChannel;
typedef struct _SpiceDisplayChannelClass SpiceDisplayChannelClass;
typedef struct spice_display_channel spice_display_channel;

struct _SpiceDisplayChannel {
    SpiceChannel parent;
    spice_display_channel *priv;
    /* Do not add fields to this struct */
};

struct _SpiceDisplayChannelClass {
    SpiceChannelClass parent_class;

    /* signals */
    void (*display_primary_create)(SpiceChannel *channel, gint format,
                                   gint width, gint height, gint stride,
                                   gint shmid, gpointer data);
    void (*display_primary_destroy)(SpiceChannel *channel);
    void (*display_invalidate)(SpiceChannel *channel,
                               gint x, gint y, gint w, gint h);
    void (*display_mark)(SpiceChannel *channel,
                         gboolean mark);

    /*< private >*/
    /* Do not add fields to this struct */
};

GType	        spice_display_channel_get_type(void);

G_END_DECLS

#endif /* __SPICE_CLIENT_DISPLAY_CHANNEL_H__ */
