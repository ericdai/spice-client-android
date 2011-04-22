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

#include "spice-session-priv.h"
#include "spice-channel-priv.h"

/* coroutine context */
G_GNUC_INTERNAL
void spice_channel_handle_set_ack(SpiceChannel *channel, spice_msg_in *in)
{
    spice_channel *c = channel->priv;
    SpiceMsgSetAck* ack = spice_msg_in_parsed(in);
    spice_msg_out *out = spice_msg_out_new(channel, SPICE_MSGC_ACK_SYNC);
    SpiceMsgcAckSync sync = {
        .generation = ack->generation,
    };

    c->message_ack_window = c->message_ack_count = ack->window;
    c->marshallers->msgc_ack_sync(out->marshaller, &sync);
    spice_msg_out_send_internal(out);
    spice_msg_out_unref(out);
}

/* coroutine context */
G_GNUC_INTERNAL
void spice_channel_handle_ping(SpiceChannel *channel, spice_msg_in *in)
{
    spice_channel *c = channel->priv;
    SpiceMsgPing *ping = spice_msg_in_parsed(in);
    spice_msg_out *pong = spice_msg_out_new(channel, SPICE_MSGC_PONG);

    c->marshallers->msgc_pong(pong->marshaller, ping);
    spice_msg_out_send_internal(pong);
    spice_msg_out_unref(pong);
}

/* coroutine context */
G_GNUC_INTERNAL
void spice_channel_handle_notify(SpiceChannel *channel, spice_msg_in *in)
{
    spice_channel *c = channel->priv;
    static const char* severity_strings[] = {"info", "warn", "error"};
    static const char* visibility_strings[] = {"!", "!!", "!!!"};

    SpiceMsgNotify *notify = spice_msg_in_parsed(in);
    const char *severity   = "?";
    const char *visibility = "?";
    const char *message_str = NULL;

    if (notify->severity <= SPICE_NOTIFY_SEVERITY_ERROR) {
        severity = severity_strings[notify->severity];
    }
    if (notify->visibilty <= SPICE_NOTIFY_VISIBILITY_HIGH) {
        visibility = visibility_strings[notify->visibilty];
    }

    if (notify->message_len &&
        notify->message_len <= in->dpos - sizeof(*notify)) {
        message_str = (char*)notify->message;
    }

    SPICE_DEBUG("%s: channel %s -- %s%s #%u%s%.*s", __FUNCTION__,
            c->name, severity, visibility, notify->what,
            message_str ? ": " : "", notify->message_len,
            message_str ? message_str : "");
}

/* coroutine context */
G_GNUC_INTERNAL
void spice_channel_handle_disconnect(SpiceChannel *channel, spice_msg_in *in)
{
    SpiceMsgDisconnect *disconnect = spice_msg_in_parsed(in);

    SPICE_DEBUG("%s: ts: %" PRIu64", reason: %u", __FUNCTION__,
                disconnect->time_stamp, disconnect->reason);
}

/* coroutine context */
G_GNUC_INTERNAL
void spice_channel_handle_wait_for_channels(SpiceChannel *channel, spice_msg_in *in)
{
    /* spice_channel *c = channel->priv;
       SpiceMsgWaitForChannels *wfc = spice_msg_in_parsed(in); */

    SPICE_DEBUG("%s TODO", __FUNCTION__);
}

static void
get_msg_handler(SpiceChannel *channel, spice_msg_in *in, gpointer data)
{
    spice_msg_in **msg = data;

    g_return_if_fail(msg != NULL);
    g_return_if_fail(*msg == NULL);

    spice_msg_in_ref(in);
    *msg = in;
}

/* coroutine context */
G_GNUC_INTERNAL
void spice_channel_handle_migrate(SpiceChannel *channel, spice_msg_in *in)
{
    spice_msg_out *out;
    spice_msg_in *data = NULL;
    SpiceMsgMigrate *mig = spice_msg_in_parsed(in);
    spice_channel *c = channel->priv;

    SPICE_DEBUG("%s: channel %s flags %u", __FUNCTION__, c->name, mig->flags);
    if (mig->flags & SPICE_MIGRATE_NEED_FLUSH) {
        /* iterate_write is blocking and flushing all pending write */
        SPICE_CHANNEL_GET_CLASS(channel)->iterate_write(channel);

        out = spice_msg_out_new(SPICE_CHANNEL(channel), SPICE_MSGC_MIGRATE_FLUSH_MARK);
        spice_msg_out_send_internal(out);
        spice_msg_out_unref(out);
        SPICE_CHANNEL_GET_CLASS(channel)->iterate_write(channel);
    }
    if (mig->flags & SPICE_MIGRATE_NEED_DATA_TRANSFER) {
        spice_channel_recv_msg(channel, get_msg_handler, &data);
        if (!data || data->header.type != SPICE_MSG_MIGRATE_DATA) {
            g_warning("expected SPICE_MSG_MIGRATE_DATA, got %d", data->header.type);
        }
    }

    spice_session_channel_migrate(c->session, channel);

    if (mig->flags & SPICE_MIGRATE_NEED_DATA_TRANSFER) {
        out = spice_msg_out_new(SPICE_CHANNEL(channel), SPICE_MSGC_MIGRATE_DATA);
        spice_marshaller_add(out->marshaller, data->data, data->header.size);
        spice_msg_out_send_internal(out);
        spice_msg_out_unref(out);
    }
}
