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
#ifndef __SPICE_CLIENT_INPUTS_CHANNEL_H__
#define __SPICE_CLIENT_INPUTS_CHANNEL_H__

#include "spice-client.h"

G_BEGIN_DECLS

#define SPICE_TYPE_INPUTS_CHANNEL            (spice_inputs_channel_get_type())
#define SPICE_INPUTS_CHANNEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SPICE_TYPE_INPUTS_CHANNEL, SpiceInputsChannel))
#define SPICE_INPUTS_CHANNEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SPICE_TYPE_INPUTS_CHANNEL, SpiceInputsChannelClass))
#define SPICE_IS_INPUTS_CHANNEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPICE_TYPE_INPUTS_CHANNEL))
#define SPICE_IS_INPUTS_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPICE_TYPE_INPUTS_CHANNEL))
#define SPICE_INPUTS_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), SPICE_TYPE_INPUTS_CHANNEL, SpiceInputsChannelClass))

typedef struct _SpiceInputsChannel SpiceInputsChannel;
typedef struct _SpiceInputsChannelClass SpiceInputsChannelClass;
typedef struct spice_inputs_channel spice_inputs_channel;

typedef enum {
    SPICE_INPUTS_SCROLL_LOCK = (1 << 0),
    SPICE_INPUTS_NUM_LOCK    = (1 << 1),
    SPICE_INPUTS_CAPS_LOCK   = (1 << 2)
} SpiceInputsLock;

struct _SpiceInputsChannel {
    SpiceChannel parent;
    spice_inputs_channel *priv;
    /* Do not add fields to this struct */
};

struct _SpiceInputsChannelClass {
    SpiceChannelClass parent_class;

    /* signals */
    void (*inputs_modifiers)(SpiceChannel *channel);

    /*< private >*/
    /* Do not add fields to this struct */
};

GType spice_inputs_channel_get_type(void);

void spice_inputs_motion(SpiceInputsChannel *channel, gint dx, gint dy,
                         gint button_state);
void spice_inputs_position(SpiceInputsChannel *channel, gint x, gint y,
                           gint display, gint button_state);
void spice_inputs_button_press(SpiceInputsChannel *channel, gint button,
                               gint button_state);
void spice_inputs_button_release(SpiceInputsChannel *channel, gint button,
                                 gint button_state);
void spice_inputs_key_press(SpiceInputsChannel *channel, guint keyval);
void spice_inputs_key_release(SpiceInputsChannel *channel, guint keyval);
void spice_inputs_set_key_locks(SpiceInputsChannel *channel, guint locks);

G_END_DECLS

#endif /* __SPICE_CLIENT_INPUTS_CHANNEL_H__ */
