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

/**
 * SECTION:channel-inputs
 * @short_description: control the server mouse and keyboard
 * @title: Inputs Channel
 * @section_id:
 * @see_also: #SpiceChannel, and the GTK widget #SpiceDisplay
 * @stability: Stable
 * @include: channel-inputs.h
 *
 * Spice supports sending keyboard key events and keyboard leds
 * synchronization. The key events are sent using
 * spice_inputs_key_press() and spice_inputs_key_release() using PC AT
 * scancode.
 *
 * Guest keyboard leds state can be manipulated with
 * spice_inputs_set_key_locks(). When key lock change, a notification
 * is emitted with #SpiceInputsChannel::inputs-modifiers signal.
 */

#define SPICE_INPUTS_CHANNEL_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_INPUTS_CHANNEL, spice_inputs_channel))

struct spice_inputs_channel {
    int                         bs;
    int                         dx, dy;
    unsigned int                x, y, dpy;
    int                         motion_count;
    int                         modifiers;
    guint32                     locks;
};

G_DEFINE_TYPE(SpiceInputsChannel, spice_inputs_channel, SPICE_TYPE_CHANNEL)

/* Properties */
enum {
    PROP_0,
    PROP_KEY_MODIFIERS,
};

/* Signals */
enum {
    SPICE_INPUTS_MODIFIERS,

    SPICE_INPUTS_LAST_SIGNAL,
};

static guint signals[SPICE_INPUTS_LAST_SIGNAL];

static void spice_inputs_handle_msg(SpiceChannel *channel, spice_msg_in *msg);
static void spice_inputs_channel_up(SpiceChannel *channel);

/* ------------------------------------------------------------------ */

static void spice_inputs_channel_init(SpiceInputsChannel *channel)
{
    spice_inputs_channel *c;

    c = channel->priv = SPICE_INPUTS_CHANNEL_GET_PRIVATE(channel);
    memset(c, 0, sizeof(*c));
}

static void spice_inputs_get_property(GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
    spice_inputs_channel *c = SPICE_INPUTS_CHANNEL(object)->priv;

    switch (prop_id) {
    case PROP_KEY_MODIFIERS:
        g_value_set_int(value, c->modifiers);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void spice_inputs_channel_finalize(GObject *obj)
{
    if (G_OBJECT_CLASS(spice_inputs_channel_parent_class)->finalize)
        G_OBJECT_CLASS(spice_inputs_channel_parent_class)->finalize(obj);
}

static void spice_inputs_channel_class_init(SpiceInputsChannelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    SpiceChannelClass *channel_class = SPICE_CHANNEL_CLASS(klass);

    gobject_class->finalize     = spice_inputs_channel_finalize;
    gobject_class->get_property = spice_inputs_get_property;
    channel_class->handle_msg   = spice_inputs_handle_msg;
    channel_class->channel_up   = spice_inputs_channel_up;

    g_object_class_install_property
        (gobject_class, PROP_KEY_MODIFIERS,
         g_param_spec_int("key-modifiers",
                          "Key modifiers",
                          "Guest keyboard lock/led state",
                          0, INT_MAX, 0,
                          G_PARAM_READABLE |
                          G_PARAM_STATIC_NAME |
                          G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

    /**
     * SpiceInputsChannel::inputs-modifier:
     * @display: the #SpiceInputsChannel that emitted the signal
     *
     * The #SpiceInputsChannel::inputs-modifier signal is emitted when
     * the guest keyboard locks are changed. You can read the current
     * state from #SpiceInputsChannel:key-modifiers property.
     **/
    /* TODO: use notify instead? */
    signals[SPICE_INPUTS_MODIFIERS] =
        g_signal_new("inputs-modifiers",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceInputsChannelClass, inputs_modifiers),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    g_type_class_add_private(klass, sizeof(spice_inputs_channel));
}

/* signal trampoline---------------------------------------------------------- */

struct SPICE_INPUTS_MODIFIERS {
};

/* main context */
static void do_emit_main_context(GObject *object, int signum, gpointer params)
{
    switch (signum) {
    case SPICE_INPUTS_MODIFIERS: {
        g_signal_emit(object, signals[signum], 0);
        break;
    }
    default:
        g_warn_if_reached();
    }
}

/* ------------------------------------------------------------------ */

static spice_msg_out* mouse_motion(SpiceInputsChannel *channel)
{
    spice_inputs_channel *c = channel->priv;
    SpiceMsgcMouseMotion motion;
    spice_msg_out *msg;

    if (!c->dx && !c->dy)
        return NULL;

    motion.buttons_state = c->bs;
    motion.dx            = c->dx;
    motion.dy            = c->dy;
    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_MOUSE_MOTION);
    msg->marshallers->msgc_inputs_mouse_motion(msg->marshaller, &motion);

    c->motion_count++;
    c->dx = 0;
    c->dy = 0;

    return msg;
}

static spice_msg_out* mouse_position(SpiceInputsChannel *channel)
{
    spice_inputs_channel *c = channel->priv;
    SpiceMsgcMousePosition position;
    spice_msg_out *msg;

    if (c->dpy == -1)
        return NULL;

    /* SPICE_DEBUG("%s: +%d+%d", __FUNCTION__, c->x, c->y); */
    position.buttons_state = c->bs;
    position.x             = c->x;
    position.y             = c->y;
    position.display_id    = c->dpy;
    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_MOUSE_POSITION);
    msg->marshallers->msgc_inputs_mouse_position(msg->marshaller, &position);

    c->motion_count++;
    c->dpy = -1;

    return msg;
}

/* main context */
static void send_position(SpiceInputsChannel *channel)
{
    spice_msg_out *msg;

    msg = mouse_position(channel);
    if (!msg) /* if no motion */
        return;

    spice_msg_out_send(msg);
    spice_msg_out_unref(msg);
}

/* main context */
static void send_motion(SpiceInputsChannel *channel)
{
    spice_msg_out *msg;

    msg = mouse_motion(channel);
    if (!msg) /* if no motion */
        return;

    spice_msg_out_send(msg);
    spice_msg_out_unref(msg);
}

/* coroutine context */
static void inputs_handle_init(SpiceChannel *channel, spice_msg_in *in)
{
    spice_inputs_channel *c = SPICE_INPUTS_CHANNEL(channel)->priv;
    SpiceMsgInputsInit *init = spice_msg_in_parsed(in);

    c->modifiers = init->keyboard_modifiers;
    emit_main_context(channel, SPICE_INPUTS_MODIFIERS);
}

/* coroutine context */
static void inputs_handle_modifiers(SpiceChannel *channel, spice_msg_in *in)
{
    spice_inputs_channel *c = SPICE_INPUTS_CHANNEL(channel)->priv;
    SpiceMsgInputsKeyModifiers *modifiers = spice_msg_in_parsed(in);

    c->modifiers = modifiers->modifiers;
    emit_main_context(channel, SPICE_INPUTS_MODIFIERS);
}

/* coroutine context */
static void inputs_handle_ack(SpiceChannel *channel, spice_msg_in *in)
{
    SPICE_DEBUG("---------------\nGot inputs_handle_ack");
    spice_inputs_channel *c = SPICE_INPUTS_CHANNEL(channel)->priv;
    spice_msg_out *msg;

    c->motion_count -= SPICE_INPUT_MOTION_ACK_BUNCH;

    msg = mouse_motion(SPICE_INPUTS_CHANNEL(channel));
    if (msg) { /* if no motion, msg == NULL */
        spice_msg_out_send_internal(msg);
        spice_msg_out_unref(msg);
    }

    msg = mouse_position(SPICE_INPUTS_CHANNEL(channel));
    if (msg) {
        spice_msg_out_send_internal(msg);
        spice_msg_out_unref(msg);
    }
}

static spice_msg_handler inputs_handlers[] = {
    [ SPICE_MSG_SET_ACK ]                  = spice_channel_handle_set_ack,
    [ SPICE_MSG_PING ]                     = spice_channel_handle_ping,
    [ SPICE_MSG_NOTIFY ]                   = spice_channel_handle_notify,
    [ SPICE_MSG_DISCONNECTING ]            = spice_channel_handle_disconnect,
    [ SPICE_MSG_WAIT_FOR_CHANNELS ]        = spice_channel_handle_wait_for_channels,
    [ SPICE_MSG_MIGRATE ]                  = spice_channel_handle_migrate,

    [ SPICE_MSG_INPUTS_INIT ]              = inputs_handle_init,
    [ SPICE_MSG_INPUTS_KEY_MODIFIERS ]     = inputs_handle_modifiers,
    [ SPICE_MSG_INPUTS_MOUSE_MOTION_ACK ]  = inputs_handle_ack,
};

/* coroutine context */
static void spice_inputs_handle_msg(SpiceChannel *channel, spice_msg_in *msg)
{
    int type = spice_msg_in_type(msg);
    g_return_if_fail(type < SPICE_N_ELEMENTS(inputs_handlers));
    g_return_if_fail(inputs_handlers[type] != NULL);
    inputs_handlers[type](channel, msg);
}

/**
 * spice_inputs_motion:
 * @channel:
 * @dx: delta X mouse coordinates
 * @dy: delta Y mouse coordinates
 * @button_state: SPICE_MOUSE_BUTTON_MASK flags
 *
 * Change mouse position (used in SPICE_MOUSE_MODE_CLIENT).
 **/
void spice_inputs_motion(SpiceInputsChannel *channel, gint dx, gint dy,
                         gint button_state)
{
    spice_inputs_channel *c;

    g_return_if_fail(channel != NULL);
    g_return_if_fail(SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_UNCONNECTED);
    if (SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_READY)
        return;

    c = channel->priv;
    c->bs  = button_state;
    c->dx += dx;
    c->dy += dy;

    if (c->motion_count < SPICE_INPUT_MOTION_ACK_BUNCH * 2) {
        send_motion(channel);
    }
}

/**
 * spice_inputs_position:
 * @channel:
 * @x: X mouse coordinates
 * @y: Y mouse coordinates
 * @display: display channel id
 * @button_state: SPICE_MOUSE_BUTTON_MASK flags
 *
 * Change mouse position (used in SPICE_MOUSE_MODE_CLIENT).
 **/
void spice_inputs_position(SpiceInputsChannel *channel, gint x, gint y,
                           gint display, gint button_state)
{
    spice_inputs_channel *c;

    g_return_if_fail(channel != NULL);

    if (SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_READY)
        return;

    c = channel->priv;
    c->bs  = button_state;
    c->x   = x;
    c->y   = y;
    c->dpy = display;

    if (c->motion_count < SPICE_INPUT_MOTION_ACK_BUNCH * 2) {
        send_position(channel);
    } else {
        SPICE_DEBUG("over SPICE_INPUT_MOTION_ACK_BUNCH * 2, dropping");
    }
}

/**
 * spice_inputs_button_press:
 * @channel:
 * @button: a SPICE_MOUSE_BUTTON
 * @button_state: SPICE_MOUSE_BUTTON_MASK flags
 *
 * Press a mouse button.
 **/
void spice_inputs_button_press(SpiceInputsChannel *channel, gint button,
                               gint button_state)
{
    spice_inputs_channel *c;
    SpiceMsgcMousePress press;
    spice_msg_out *msg;

    g_return_if_fail(channel != NULL);

    if (SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_READY)
        return;

    c = channel->priv;
    switch (button) {
    case SPICE_MOUSE_BUTTON_LEFT:
        button_state |= SPICE_MOUSE_BUTTON_MASK_LEFT;
        break;
    case SPICE_MOUSE_BUTTON_MIDDLE:
        button_state |= SPICE_MOUSE_BUTTON_MASK_MIDDLE;
        break;
    case SPICE_MOUSE_BUTTON_RIGHT:
        button_state |= SPICE_MOUSE_BUTTON_MASK_RIGHT;
        break;
    }

    c->bs  = button_state;
    //send_motion(channel);
    //send_position(channel);

    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_MOUSE_PRESS);
    press.button = button;
    press.buttons_state = button_state;
    msg->marshallers->msgc_inputs_mouse_press(msg->marshaller, &press);
    spice_msg_out_send(msg);
    spice_msg_out_unref(msg);
}

/**
 * spice_inputs_button_release:
 * @channel:
 * @button: a SPICE_MOUSE_BUTTON
 * @button_state: SPICE_MOUSE_BUTTON_MASK flags
 *
 * Release a button.
 **/
void spice_inputs_button_release(SpiceInputsChannel *channel, gint button,
                                 gint button_state)
{
    spice_inputs_channel *c;
    SpiceMsgcMouseRelease release;
    spice_msg_out *msg;

    g_return_if_fail(channel != NULL);

    if (SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_READY)
        return;

    c = channel->priv;
    switch (button) {
    case SPICE_MOUSE_BUTTON_LEFT:
        button_state &= ~SPICE_MOUSE_BUTTON_MASK_LEFT;
        break;
    case SPICE_MOUSE_BUTTON_MIDDLE:
        button_state &= ~SPICE_MOUSE_BUTTON_MASK_MIDDLE;
        break;
    case SPICE_MOUSE_BUTTON_RIGHT:
        button_state &= ~SPICE_MOUSE_BUTTON_MASK_RIGHT;
        break;
    }

    c->bs  = button_state;
    //send_motion(channel);
    //send_position(channel);

    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_MOUSE_RELEASE);
    release.button = button;
    release.buttons_state = button_state;
    msg->marshallers->msgc_inputs_mouse_release(msg->marshaller, &release);
    spice_msg_out_send(msg);
    spice_msg_out_unref(msg);
}

/**
 * spice_inputs_key_press:
 * @channel:
 * @scancode: a PC AT key scancode
 *
 * Press a key.
 **/
void spice_inputs_key_press(SpiceInputsChannel *channel, guint scancode)
{
    SpiceMsgcKeyDown down;
    spice_msg_out *msg;

    g_return_if_fail(channel != NULL);
    g_return_if_fail(SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_UNCONNECTED);
    if (SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_READY)
        return;

    SPICE_DEBUG("%s: scancode %d", __FUNCTION__, scancode);
    if (scancode < 0x100) {
        down.code = scancode;
    } else {
        down.code = 0xe0 | ((scancode - 0x100) << 8);
    }

    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_KEY_DOWN);
    msg->marshallers->msgc_inputs_key_down(msg->marshaller, &down);
    spice_msg_out_send(msg);
    spice_msg_out_unref(msg);
}

/**
 * spice_inputs_key_release:
 * @channel:
 * @scancode: a PC AT key scancode
 *
 * Release a key.
 **/
void spice_inputs_key_release(SpiceInputsChannel *channel, guint scancode)
{
    SpiceMsgcKeyUp up;
    spice_msg_out *msg;

    g_return_if_fail(channel != NULL);
    g_return_if_fail(SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_UNCONNECTED);
    if (SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_READY)
        return;

    SPICE_DEBUG("%s: scancode %d", __FUNCTION__, scancode);
    if (scancode < 0x100) {
        up.code = scancode | 0x80;
    } else {
        up.code = 0x80e0 | ((scancode - 0x100) << 8);
    }

    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_KEY_UP);
    msg->marshallers->msgc_inputs_key_up(msg->marshaller, &up);
    spice_msg_out_send(msg);
    spice_msg_out_unref(msg);
}

/* main or coroutine context */
static spice_msg_out* set_key_locks(SpiceInputsChannel *channel, guint locks)
{
    SpiceMsgcKeyModifiers modifiers;
    spice_msg_out *msg;
    spice_inputs_channel *ic;
    spice_channel *c;

    g_return_val_if_fail(SPICE_IS_INPUTS_CHANNEL(channel), NULL);

    ic = channel->priv;
    c = SPICE_CHANNEL(channel)->priv;

    ic->locks = locks;
    if (c->state != SPICE_CHANNEL_STATE_READY)
        return NULL;

    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_KEY_MODIFIERS);
    modifiers.modifiers = locks;
    msg->marshallers->msgc_inputs_key_modifiers(msg->marshaller, &modifiers);
    return msg;
}

/**
 * spice_inputs_set_key_locks:
 * @channel:
 * @locks: #SpiceInputsLock modifiers flags
 *
 * Set the keyboard locks on the guest (Caps, Num, Scroll..)
 **/
void spice_inputs_set_key_locks(SpiceInputsChannel *channel, guint locks)
{
    spice_msg_out *msg;

    msg = set_key_locks(channel, locks);
    if (!msg) /* you can set_key_locks() even if the channel is not ready */
        return;

    spice_msg_out_send(msg); /* main -> coroutine */
    spice_msg_out_unref(msg);
}

/* coroutine context */
static void spice_inputs_channel_up(SpiceChannel *channel)
{
    spice_inputs_channel *c = SPICE_INPUTS_CHANNEL(channel)->priv;
    spice_msg_out *msg;

    msg = set_key_locks(SPICE_INPUTS_CHANNEL(channel), c->locks);
    spice_msg_out_send_internal(msg);
    spice_msg_out_unref(msg);
}
