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

#include <gst/audio/audio.h>
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

void tcpserver_set ( TcpServer *tcpserver, const char *host, uint port, const char *file )
{
	g_object_set ( tcpserver->file_src, "location", file, NULL );
	g_object_set ( tcpserver->server_sink, "host",  host, NULL );
	g_object_set ( tcpserver->server_sink, "port",  port, NULL );
}

uint tcpserver_get ( TcpServer *tcpserver )
{
	uint num_cl = 0;
	g_object_get ( tcpserver->server_sink, "num-handles", &num_cl, NULL );

	return num_cl;
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

static bool tcpserver_init ( TcpServer *tcpserver )
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

TcpServer * tcpserver_new ()
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

static bool record_init ( Record *record )
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

Record * record_new ()
{
	Record *record = g_new0 ( Record, 1 );

	if ( record_init ( record ) ) return record; else free ( record );

	return NULL;
}



static void player_set_next_track ( const char *name_n, const char *name_c, Player *player )
{
	bool audio = FALSE;
	if ( g_str_has_suffix ( name_n, "audio" ) )
	{
		audio = TRUE;
		bool mute = FALSE;
		g_object_get ( player->playbin, "mute", &mute, NULL );
		if ( mute ) { g_object_set ( player->playbin, "mute",  !mute, NULL ); return; }
	}

	int index = 0, n_track = 0, current = 0;
	g_object_get ( player->playbin, name_n, &n_track, NULL );
	g_object_get ( player->playbin, name_c, &current, NULL );

	index = current + 1;
	if ( n_track == index ) { if ( audio ) g_object_set ( player->playbin, "mute",  TRUE, NULL ); index = 0; }
	g_object_set ( player->playbin, name_c, index, NULL );

	// g_message ( "%s:: Switching to %s track %d ", __func__, name_c, index );
}

static void player_video_press_event ( int button, Player *player )
{
	if ( button == 1 ) player_set_next_track ( "n-audio", "current-audio", player );
	if ( button == 2 ) player_set_next_track ( "n-video", "current-video", player );
	if ( button == 3 ) player_set_next_track ( "n-text",  "current-text",  player );
}

static void player_msg_element ( G_GNUC_UNUSED GstBus *bus, GstMessage *msg, Player *player )
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
				case GST_NAVIGATION_EVENT_KEY_PRESS:
				{
					const char *key_i = NULL;

					if ( gst_navigation_event_parse_key_event (ev, &key_i) )
					{
						char key = '\0';

						if ( key_i[0] != '\0' && key_i[1] == '\0' )
							key = g_ascii_tolower (key_i[0]);

						// g_message ( "%s:: key %s", __func__, key_i );

						switch (key)
						{
							case 'a':
								player_set_next_track ( "n-audio", "current-audio", player );
								break;
							case 'v':
								player_set_next_track ( "n-video", "current-video", player );
								break;
							case 's':
								player_set_next_track ( "n-text", "current-text", player );
								break;
							default:
								break;
						}
					}
					break;
				}

				case GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS:
				{
					int button;
					if ( gst_navigation_event_parse_mouse_button_event ( ev, &button, NULL, NULL ) )
					{
						player_video_press_event ( button, player );
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

static bool player_init ( bool add_time, Player *player )
{
	player->playbin = gst_element_factory_make ( "playbin", NULL );

	if ( !player->playbin )
	{
		g_critical ( "%s:: player playbin - not be created.\n", __func__ );
		return FALSE;
	}

	enum gst_flags { GST_FLAG_VIS = (1 << 3) };

	uint flags;
	g_object_get ( player->playbin, "flags", &flags, NULL );

	flags |= GST_FLAG_VIS;
	g_object_set ( player->playbin, "flags", flags, NULL );

	GstElement *visual_goom = gst_element_factory_make ( "goom", NULL );
	if ( visual_goom ) g_object_set ( player->playbin, "vis-plugin", visual_goom, NULL );

	GstElement *videoconvert  = gst_element_factory_make ( "videoconvert",  NULL );
	GstElement *autovideosink = gst_element_factory_make ( "autovideosink", NULL );
	GstElement *timeoverlay   = gst_element_factory_make ( "timeoverlay",   NULL );

	if ( !videoconvert || !autovideosink || !timeoverlay )
	{
		g_critical ( "%s:: pipeline player - not be created.\n", __func__ );
		return FALSE;
	}

	GstElement *bin_video = gst_bin_new ( "video-sink-bin" );

	( add_time ) ? gst_bin_add_many ( GST_BIN ( bin_video ), videoconvert, timeoverlay, autovideosink, NULL )
		     : gst_bin_add_many ( GST_BIN ( bin_video ), videoconvert, autovideosink, NULL );

	( add_time ) ? gst_element_link_many ( videoconvert, timeoverlay, autovideosink, NULL )
		     : gst_element_link_many ( videoconvert, autovideosink, NULL );

	GstPad *padv = gst_element_get_static_pad ( videoconvert, "sink" );
	gst_element_add_pad ( bin_video, gst_ghost_pad_new ( "sink", padv ) );
	gst_object_unref ( padv );

	g_object_set ( player->playbin, "video-sink", bin_video, NULL );

	GstBus *bus = gst_element_get_bus ( player->playbin );
	gst_bus_add_signal_watch ( bus );

	g_signal_connect ( bus, "message::error", G_CALLBACK ( player_msg_err ), player );
	g_signal_connect ( bus, "message::element", G_CALLBACK ( player_msg_element ), player );
	gst_object_unref ( bus );

	return TRUE;
}

void player_destroy ( Player *player )
{
	gst_element_set_state ( player->playbin, GST_STATE_NULL );

	gst_object_unref ( player->playbin );
	free ( player );
}

Player * player_new ( bool add_time )
{
	Player *player = g_new0 ( Player, 1 );

	if ( player_init ( add_time, player ) ) return player; else free ( player );

	return NULL;
}

#endif
