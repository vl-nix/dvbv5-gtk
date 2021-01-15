/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef LIGHT

#include "zap-gst.h"

#include <gst/video/video.h>

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>


static void message_dialog ( const char *error, const char *file_or_info, GtkMessageType mesg_type )
{
	GtkMessageDialog *dialog = ( GtkMessageDialog *)gtk_message_dialog_new (
		NULL, GTK_DIALOG_MODAL, mesg_type, GTK_BUTTONS_CLOSE, "%s\n%s", error, file_or_info );

	gtk_dialog_run     ( GTK_DIALOG ( dialog ) );
	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
}


void tcpserver_stop  ( TcpServer *tcpserver )
{
	gst_element_set_state ( tcpserver->pipeline, GST_STATE_NULL );
}

void tcpserver_start ( TcpServer *tcpserver )
{
	gst_element_set_state ( tcpserver->pipeline, GST_STATE_PLAYING );
}

void tcpserver_set ( TcpServer *tcpserver, const char *host, u_int16_t port, const char *file )
{
	g_object_set ( tcpserver->file_src, "location", file, NULL );
	g_object_set ( tcpserver->server_sink, "host",  host, NULL );
	g_object_set ( tcpserver->server_sink, "port",  port, NULL );
}

static void tcpserver_msg_err ( G_GNUC_UNUSED GstBus *bus, GstMessage *msg, TcpServer *tcpserver )
{
	gst_element_set_state ( tcpserver->pipeline, GST_STATE_NULL );

	GError *err = NULL;
	char   *dbg = NULL;

	gst_message_parse_error ( msg, &err, &dbg );

	g_critical ( "%s:: %s (%s)\n", __func__, err->message, (dbg) ? dbg : "no details" );
	message_dialog ( "Server", err->message, GTK_MESSAGE_ERROR );

	g_error_free ( err );
	g_free ( dbg );
}

static gboolean tcpserver_init ( TcpServer *tcpserver )
{
	tcpserver->pipeline    = gst_pipeline_new ( "pipeline-server" );
	tcpserver->file_src    = gst_element_factory_make ( "filesrc",       NULL );
	tcpserver->server_sink = gst_element_factory_make ( "tcpserversink", NULL );

	if ( !tcpserver->pipeline || !tcpserver->file_src || !tcpserver->server_sink )
	{
		g_critical ( "%s:: pipeline server - not be created.\n", __func__ );
		return FALSE;
	}

	gst_bin_add_many ( GST_BIN ( tcpserver->pipeline ), tcpserver->file_src, tcpserver->server_sink, NULL );
	gst_element_link_many ( tcpserver->file_src, tcpserver->server_sink, NULL );

	GstBus *bus = gst_element_get_bus ( tcpserver->pipeline );
	gst_bus_add_signal_watch ( bus );

	g_signal_connect ( bus, "message::error", G_CALLBACK ( tcpserver_msg_err ), tcpserver );
	gst_object_unref ( bus );

	return TRUE;
}

void tcpserver_destroy ( TcpServer *tcpserver )
{
	gst_element_set_state ( tcpserver->pipeline, GST_STATE_NULL );
	gst_bin_remove ( GST_BIN ( tcpserver->pipeline ), tcpserver->file_src );
	gst_bin_remove ( GST_BIN ( tcpserver->pipeline ), tcpserver->server_sink );

	gst_object_unref ( tcpserver->pipeline );
	free ( tcpserver );
}

TcpServer * tcpserver_new ( void )
{
	TcpServer *tcpserver = g_new0 ( TcpServer, 1 );

	if ( tcpserver_init ( tcpserver ) ) return tcpserver; else free ( tcpserver );

	return NULL;
}



void record_stop  ( Record *record )
{
	gst_element_set_state ( record->pipeline, GST_STATE_NULL );
}

void record_start ( Record *record )
{
	gst_element_set_state ( record->pipeline, GST_STATE_PLAYING );
}

void record_set ( Record *record, const char *file, const char *file_rec )
{
	g_object_set ( record->file_src,  "location", file,     NULL );
	g_object_set ( record->file_sink, "location", file_rec, NULL );
}

static void record_msg_err ( G_GNUC_UNUSED GstBus *bus, GstMessage *msg, Record *record )
{
	gst_element_set_state ( record->pipeline, GST_STATE_NULL );

	GError *err = NULL;
	char   *dbg = NULL;

	gst_message_parse_error ( msg, &err, &dbg );

	g_critical ( "%s:: %s (%s)\n", __func__, err->message, (dbg) ? dbg : "no details" );
	message_dialog ( "Record", err->message, GTK_MESSAGE_ERROR );

	g_error_free ( err );
	g_free ( dbg );
}

static gboolean record_init ( Record *record )
{
	record->pipeline  = gst_pipeline_new ( "pipeline-record" );
	record->file_src  = gst_element_factory_make ( "filesrc",  NULL );
	record->file_sink = gst_element_factory_make ( "filesink", NULL );

	if ( !record->pipeline || !record->file_src || !record->file_sink )
	{
		g_critical ( "%s:: pipeline record - not be created.\n", __func__ );
		return FALSE;
	}

	gst_bin_add_many ( GST_BIN ( record->pipeline ), record->file_src, record->file_sink, NULL );
	gst_element_link_many ( record->file_src, record->file_sink, NULL );

	GstBus *bus = gst_element_get_bus ( record->pipeline );
	gst_bus_add_signal_watch ( bus );

	g_signal_connect ( bus, "message::error", G_CALLBACK ( record_msg_err ), record );
	gst_object_unref ( bus );

	return TRUE;
}

void record_destroy ( Record *record )
{
	gst_element_set_state ( record->pipeline, GST_STATE_NULL );
	gst_bin_remove ( GST_BIN ( record->pipeline ), record->file_src );
	gst_bin_remove ( GST_BIN ( record->pipeline ), record->file_sink );

	gst_object_unref ( record->pipeline );
	free ( record );
}

Record * record_new ( void )
{
	Record *record = g_new0 ( Record, 1 );

	if ( record_init ( record ) ) return record; else free ( record );

	return NULL;
}



static void player_set_mute ( GstElement *element )
{
	gboolean mute = FALSE;
	g_object_get ( element, "mute", &mute, NULL );
	g_object_set ( element, "mute", !mute, NULL );
}

static void player_mouse_button_event ( int button, GstElement *element )
{
	if ( button == 3 ) player_set_mute ( element );
}

static void player_msg_element ( G_GNUC_UNUSED GstBus *bus, GstMessage *msg, GstElement *element )
{
	GstNavigationMessageType mtype = gst_navigation_message_get_type (msg);

	if ( mtype == GST_NAVIGATION_MESSAGE_EVENT )
	{
		GstEvent *ev = NULL;

		if ( gst_navigation_message_parse_event (msg, &ev) )
		{
			GstNavigationEventType e_type = gst_navigation_event_get_type (ev);

			switch (e_type)
			{
				case GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS:
				{
					int button;
					if ( gst_navigation_event_parse_mouse_button_event ( ev, &button, NULL, NULL ) )
					{
						player_mouse_button_event ( button, element );
					}
					break;
				}

				default:
					break;
			}
		}

		if (ev) gst_event_unref (ev);
	}
}



void player_stop  ( Player *player )
{
	gst_element_set_state ( player->playbin, GST_STATE_NULL );
}

void player_play  ( Player *player, const char *uri )
{
	gst_element_set_state ( player->playbin, GST_STATE_NULL );
	g_object_set ( player->playbin, "uri", uri, NULL );
	gst_element_set_state ( player->playbin, GST_STATE_PLAYING );
}

static void player_msg_err ( G_GNUC_UNUSED GstBus *bus, GstMessage *msg, Player *player )
{
	gst_element_set_state ( player->playbin, GST_STATE_NULL );

	GError *err = NULL;
	char   *dbg = NULL;

	gst_message_parse_error ( msg, &err, &dbg );

	g_critical ( "%s:: %s (%s) ", __func__, err->message, (dbg) ? dbg : "no details" );
	message_dialog ( "Player", err->message, GTK_MESSAGE_ERROR );

	g_error_free ( err );
	g_free ( dbg );
}

static void player_set_vis ( Player *player )
{
	enum gst_flags { GST_FLAG_VIS = (1 << 3) };

	uint flags;
	g_object_get ( player->playbin, "flags", &flags, NULL );

	flags |= GST_FLAG_VIS;

	GstElement *visual_goom = gst_element_factory_make ( "goom", NULL );

	if ( visual_goom )
	{
		g_object_set ( player->playbin, "flags", flags, NULL );
		g_object_set ( player->playbin, "vis-plugin", visual_goom, NULL );
	}
}

static gboolean player_init ( Player *player )
{
	player->playbin = gst_element_factory_make ( "playbin", NULL );

	if ( !player->playbin )
	{
		g_critical ( "%s:: player playbin - not be created.\n", __func__ );
		return FALSE;
	}

	player_set_vis ( player );

	GstBus *bus = gst_element_get_bus ( player->playbin );
	gst_bus_add_signal_watch ( bus );

	g_signal_connect ( bus, "message::error",   G_CALLBACK ( player_msg_err ), player );
	g_signal_connect ( bus, "message::element", G_CALLBACK ( player_msg_element ), player->playbin );
	gst_object_unref ( bus );

	return TRUE;
}

void player_destroy ( Player *player )
{
	gst_element_set_state ( player->playbin, GST_STATE_NULL );

	gst_object_unref ( player->playbin );
	free ( player );
}

Player * player_new ( void )
{
	Player *player = g_new0 ( Player, 1 );

	if ( player_init ( player ) ) return player; else free ( player );

	return NULL;
}



void player_pipe_stop  ( PlayerPipe *player )
{
	gst_element_set_state ( player->pipeline, GST_STATE_NULL );
}

void player_pipe_play  ( PlayerPipe *player, int fd )
{
	gst_element_set_state ( player->pipeline, GST_STATE_NULL );
	g_object_set ( player->fd_src, "fd", fd, NULL );
	gst_element_set_state ( player->pipeline, GST_STATE_PLAYING );
}

static void player_pipe_msg_err ( G_GNUC_UNUSED GstBus *bus, GstMessage *msg, PlayerPipe *player )
{
	gst_element_set_state ( player->pipeline, GST_STATE_NULL );

	GError *err = NULL;
	char   *dbg = NULL;

	gst_message_parse_error ( msg, &err, &dbg );

	g_critical ( "%s:: %s (%s) ", __func__, err->message, (dbg) ? dbg : "no details" );
	message_dialog ( "PlayerPipe", err->message, GTK_MESSAGE_ERROR );

	g_error_free ( err );
	g_free ( dbg );
}

static gboolean zap_gst_pad_check_type ( GstPad *pad, const char *type )
{
	gboolean ret = FALSE;

	GstCaps *caps = gst_pad_get_current_caps ( pad );

	const char *name = gst_structure_get_name ( gst_caps_get_structure ( caps, 0 ) );

	if ( g_str_has_prefix ( name, type ) ) ret = TRUE;

	gst_caps_unref (caps);

	return ret;
}

static void zap_gst_pad_add ( GstPad *pad, GstElement *element, const char *name )
{
	GstPad *pad_va_sink = gst_element_get_static_pad ( element, "sink" );

	if ( gst_pad_link ( pad, pad_va_sink ) == GST_PAD_LINK_OK )
		gst_object_unref ( pad_va_sink );
	else
		printf ( "%s:: linking decode %s - video/audio pad failed \n", __func__, name );
}

static void zap_gst_pad_audio_video ( G_GNUC_UNUSED GstElement *element, GstPad *pad, PlayerPipe *player )
{
	if ( zap_gst_pad_check_type ( pad, "audio" ) ) zap_gst_pad_add ( pad, player->aqueue2, "decode audio" );
	if ( zap_gst_pad_check_type ( pad, "video" ) ) zap_gst_pad_add ( pad, player->vqueue2, "decode video" );
}

static gboolean player_pipe_init ( PlayerPipe *player, u_int16_t vpid )
{
	player->pipeline  = gst_pipeline_new ( "pipeline-record" );
	player->fd_src    = gst_element_factory_make ( "fdsrc", NULL );

	if ( !player->pipeline )
	{
		g_critical ( "%s:: player-pipe - not be created.\n", __func__ );
		return FALSE;
	}

	player->aqueue2 = gst_element_factory_make ( "queue2", NULL );
	player->vqueue2 = gst_element_factory_make ( "queue2", NULL );

	GstElement *decodebin     = gst_element_factory_make ( "decodebin",     NULL );
	GstElement *audioconvert  = gst_element_factory_make ( "audioconvert",  NULL );
	GstElement *autoaudiosink = gst_element_factory_make ( "autoaudiosink", NULL );
	GstElement *videoconvert  = gst_element_factory_make ( "videoconvert",  NULL );
	GstElement *autovideosink = gst_element_factory_make ( "autovideosink", NULL );
	GstElement *volume        = gst_element_factory_make ( "volume",        NULL );

	if ( vpid )
	{
		gst_bin_add_many ( GST_BIN ( player->pipeline ), player->fd_src, decodebin, NULL );
		gst_bin_add_many ( GST_BIN ( player->pipeline ), player->aqueue2, audioconvert, volume, autoaudiosink, NULL );
		gst_bin_add_many ( GST_BIN ( player->pipeline ), player->vqueue2, videoconvert, autovideosink, NULL );

		gst_element_link_many ( player->fd_src, decodebin, NULL );
		gst_element_link_many ( player->aqueue2, audioconvert, volume, autoaudiosink, NULL );
		gst_element_link_many ( player->vqueue2, videoconvert, autovideosink, NULL );

		g_signal_connect ( decodebin, "pad-added", G_CALLBACK ( zap_gst_pad_audio_video ), player );
	}
	else
	{
		GstElement *tee  = gst_element_factory_make ( "tee", NULL );
		GstElement *goom = gst_element_factory_make ( "goom", NULL );
		GstElement *queue2 = gst_element_factory_make ( "queue2", NULL );

		gst_bin_add_many ( GST_BIN ( player->pipeline ), player->fd_src, decodebin, NULL );
		gst_bin_add_many ( GST_BIN ( player->pipeline ), player->aqueue2, audioconvert, tee, queue2, volume, autoaudiosink, NULL );
		gst_bin_add_many ( GST_BIN ( player->pipeline ), player->vqueue2, goom, videoconvert, autovideosink, NULL );

		gst_element_link_many ( player->fd_src, decodebin, NULL );
		gst_element_link_many ( player->aqueue2, audioconvert, tee, queue2, volume, autoaudiosink, NULL );
		gst_element_link_many ( tee, player->vqueue2, goom, videoconvert, autovideosink, NULL );

		g_signal_connect ( decodebin, "pad-added", G_CALLBACK ( zap_gst_pad_audio_video ), player );
	}

	GstBus *bus = gst_element_get_bus ( player->pipeline );
	gst_bus_add_signal_watch ( bus );

	g_signal_connect ( bus, "message::error", G_CALLBACK ( player_pipe_msg_err ), player );
	g_signal_connect ( bus, "message::element", G_CALLBACK ( player_msg_element ), volume );
	gst_object_unref ( bus );

	return TRUE;
}

static void player_pipe_remove_bin ( GstElement *pipeline )
{
	GstIterator *it = gst_bin_iterate_elements ( GST_BIN ( pipeline ) );
	GValue item = { 0, };
	gboolean done = FALSE;

	while ( !done )
	{
		switch ( gst_iterator_next ( it, &item ) )
		{
			case GST_ITERATOR_OK:
			{
				GstElement *element = GST_ELEMENT ( g_value_get_object (&item) );

				gst_element_set_state ( element, GST_STATE_NULL );
				gst_bin_remove ( GST_BIN ( pipeline ), element );

				g_value_reset (&item);

				break;
			}

			case GST_ITERATOR_RESYNC:
				gst_iterator_resync (it);
				break;

			case GST_ITERATOR_ERROR:
				done = TRUE;
				break;

			case GST_ITERATOR_DONE:
				done = TRUE;
				break;
		}
	}

	g_value_unset ( &item );
	gst_iterator_free ( it );
}

void player_pipe_destroy ( PlayerPipe *player )
{
	gst_element_set_state ( player->pipeline, GST_STATE_NULL );

	player_pipe_remove_bin ( player->pipeline );

	gst_object_unref ( player->pipeline );
	free ( player );
}

PlayerPipe * player_pipe_new ( u_int16_t vpid )
{
	PlayerPipe *player = g_new0 ( PlayerPipe, 1 );

	if ( player_pipe_init ( player, vpid ) ) return player; else free ( player );

	return NULL;
}

#endif
