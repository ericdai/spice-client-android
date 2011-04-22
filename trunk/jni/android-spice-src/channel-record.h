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
#ifndef __SPICE_CLIENT_RECORD_CHANNEL_H__
#define __SPICE_CLIENT_RECORD_CHANNEL_H__

#include "spice-client.h"

G_BEGIN_DECLS

#define SPICE_TYPE_RECORD_CHANNEL            (spice_record_channel_get_type())
#define SPICE_RECORD_CHANNEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SPICE_TYPE_RECORD_CHANNEL, SpiceRecordChannel))
#define SPICE_RECORD_CHANNEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SPICE_TYPE_RECORD_CHANNEL, SpiceRecordChannelClass))
#define SPICE_IS_RECORD_CHANNEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPICE_TYPE_RECORD_CHANNEL))
#define SPICE_IS_RECORD_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPICE_TYPE_RECORD_CHANNEL))
#define SPICE_RECORD_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), SPICE_TYPE_RECORD_CHANNEL, SpiceRecordChannelClass))

typedef struct _SpiceRecordChannel SpiceRecordChannel;
typedef struct _SpiceRecordChannelClass SpiceRecordChannelClass;
typedef struct spice_record_channel spice_record_channel;

struct _SpiceRecordChannel {
    SpiceChannel parent;
    spice_record_channel *priv;
    /* Do not add fields to this struct */
};

struct _SpiceRecordChannelClass {
    SpiceChannelClass parent_class;

    /* signals */
    void (*record_start)(SpiceRecordChannel *channel,
                         gint format, gint channels, gint freq);
    void (*record_data)(SpiceRecordChannel *channel, gpointer *data, gint size);
    void (*record_stop)(SpiceRecordChannel *channel);

    /*< private >*/
    /*
     * If adding fields to this struct, remove corresponding
     * amount of padding to avoid changing overall struct size
     */
    gchar _spice_reserved[SPICE_RESERVED_PADDING];
};

GType	        spice_record_channel_get_type(void);
void            spice_record_send_data(SpiceRecordChannel *channel, gpointer data,
                                       gsize bytes, uint32_t time);

G_END_DECLS

#endif /* __SPICE_CLIENT_RECORD_CHANNEL_H__ */
