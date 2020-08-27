/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "dvbv5.h"
#include "file.h"

/* Returns a newly-allocated string holding the result. Free with free() */
static char * zap_signal_time_to_str ()
{
	GDateTime *date = g_date_time_new_now_local ();

	char *str_time = g_date_time_format ( date, "%j-%Y-%T" );

	g_date_time_unref ( date );

	return str_time;
}

static bool zap_signal_check_dmx_out ( Dvbv5 *dvbv5 )
{
	g_autofree char *type_dmx = gtk_combo_box_text_get_active_text ( dvbv5->zap->combo_dmx );

	if ( g_str_has_suffix ( type_dmx, "DMX_OUT_DECODER" ) || g_str_has_suffix ( type_dmx, "DMX_OUT_TAP" ) ||
		 g_str_has_suffix ( type_dmx, "DMX_OUT_TSDEMUX_TAP" ) || g_str_has_suffix ( type_dmx, "DMX_OUT_OFF" ) )
	{
		 dvbv5_message_dialog ( "", "Support: DMX_OUT_TS_TAP or DMX_OUT_ALL_PIDS", GTK_MESSAGE_INFO, dvbv5->window );
		 return FALSE;
	}

	return TRUE;
}

static void zap_signal_set_sensitive ( Zap *zap)
{
	gtk_widget_set_sensitive ( GTK_WIDGET ( zap->toggled_prw ), TRUE );
	gtk_widget_set_sensitive ( GTK_WIDGET ( zap->toggled_rec ), TRUE );
	gtk_widget_set_sensitive ( GTK_WIDGET ( zap->toggled_stm ), TRUE );
	gtk_widget_set_sensitive ( GTK_WIDGET ( zap->toggled_tmo ), TRUE );
}

static bool zap_signal_check_fe_lock ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->dvb_zap && !dvbv5->fe_lock )
	{
		g_signal_emit_by_name ( dvbv5->control, "button-clicked", "stop" );
		dvbv5_message_dialog ( "Zap:", "FE_HAS_LOCK - failed", GTK_MESSAGE_WARNING, dvbv5->window );
		return FALSE;
	}

	return TRUE;
}

static bool zap_signal_check_all ( Dvbv5 *dvbv5 )
{
	if ( !dvbv5->zap->zap_status ) { g_message ( "Zap?" ); return FALSE; }
	if ( !zap_signal_check_fe_lock ( dvbv5 ) ) return FALSE;
	if ( !zap_signal_check_dmx_out ( dvbv5 ) ) return FALSE;

	return TRUE;
}

static bool zap_signal_check_stream ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->window == NULL ) return FALSE;

	if ( GST_ELEMENT_CAST ( dvbv5->server->pipeline )->current_state == GST_STATE_NULL )
	{
		zap_set_active_toggled_block ( dvbv5->zap->stm_signal_id, FALSE, dvbv5->zap->toggled_stm );
		return FALSE;
	}

	return TRUE;
}

static void zap_signal_toggled_stream ( GtkToggleButton *button, Dvbv5 *dvbv5 )
{
	if ( dvbv5->server == NULL ) { g_critical ( "Server = NULL" ); return; }

	g_timeout_add ( 1000, (GSourceFunc)zap_signal_check_stream, dvbv5 );

	if ( !zap_signal_check_all ( dvbv5 ) ) return;

	bool server = gtk_toggle_button_get_active ( button );

	if ( server )
	{
		dvbv5->zap->stm_status = TRUE;

		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->toggled_prw ), FALSE );
		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->toggled_rec ), FALSE );

		uint port = atoi ( gtk_entry_get_text ( dvbv5->entry_port ) );
		const char *host = gtk_entry_get_text ( dvbv5->entry_host );
		char *file = g_strdup_printf ( "/dev/dvb/adapter%d/dvr0", dvbv5->scan->adapter );

		tcpserver_set ( dvbv5->server, host, port, file );
		tcpserver_start ( dvbv5->server );

		free ( file );
	}
	else
	{
		tcpserver_stop ( dvbv5->server );
		dvbv5->zap->stm_status = FALSE;
		zap_signal_set_sensitive ( dvbv5->zap );
	}
}

static void zap_preview_set_inhibit ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->connect == NULL ) return;

	// dvbv5->cookie = gtk_application_inhibit ( GTK_APPLICATION ( dvbv5 ), NULL, GTK_APPLICATION_INHIBIT_SWITCH, "Dvbv5-Gtk Video" );

	GError *err = NULL;

	GVariant *reply = g_dbus_connection_call_sync ( dvbv5->connect,
						"org.freedesktop.PowerManagement",
						"/org/freedesktop/PowerManagement/Inhibit",
						"org.freedesktop.PowerManagement.Inhibit",
						"Inhibit",
						g_variant_new ("(ss)", "Dvbv5-Gtk", "Video" ),
						G_VARIANT_TYPE ("(u)"),
						G_DBUS_CALL_FLAGS_NONE,
						-1,
						NULL,
						&err );

	if ( reply != NULL )
	{
		g_variant_get ( reply, "(u)", &dvbv5->cookie, NULL );
		g_variant_unref ( reply );
	}

	if ( err )
	{
		g_warning ( "Inhibiting failed %s", err->message );
		g_error_free ( err );
	}
}

static void zap_preview_set_uninhibit ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->connect == NULL ) return;
	if ( dvbv5->cookie  ==    0 ) return;

	// gtk_application_uninhibit ( GTK_APPLICATION ( dvbv5 ), dvbv5->cookie );

	GError *err = NULL;

	GVariant *reply = g_dbus_connection_call_sync ( dvbv5->connect,
						"org.freedesktop.PowerManagement",
						"/org/freedesktop/PowerManagement/Inhibit",
						"org.freedesktop.PowerManagement.Inhibit",
						"UnInhibit",
						g_variant_new ("(u)", dvbv5->cookie),
						NULL,
						G_DBUS_CALL_FLAGS_NONE,
						-1,
						NULL,
						&err );

	if ( err )
	{
		g_warning ( "Uninhibiting failed %s", err->message );
		g_error_free ( err );
	}

	g_variant_unref ( reply );
	dvbv5->cookie = 0;
}

static bool zap_signal_check_preview ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->window == NULL ) return FALSE;

	if ( GST_ELEMENT_CAST ( dvbv5->player->playbin )->current_state == GST_STATE_NULL )
	{
		dvbv5->zap->prw_status = FALSE;
		zap_preview_set_uninhibit ( dvbv5 );
		zap_set_active_toggled_block ( dvbv5->zap->prw_signal_id, FALSE, dvbv5->zap->toggled_prw );
		return FALSE;
	}

	return TRUE;
}

static void zap_signal_toggled_preview ( GtkToggleButton *button, Dvbv5 *dvbv5 )
{
	if ( dvbv5->player == NULL ) { g_critical ( "Player = NULL" ); return; }

	g_timeout_add ( 1000, (GSourceFunc)zap_signal_check_preview, dvbv5 );

	if ( !zap_signal_check_all ( dvbv5 ) ) return;

	bool preview = gtk_toggle_button_get_active ( button );

	if ( preview )
	{
		dvbv5->zap->prw_status = TRUE;

		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->toggled_rec ), FALSE );
		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->toggled_stm ), FALSE );
		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->toggled_tmo ), FALSE );

		char *uri = g_strdup_printf ( "file:///dev/dvb/adapter%d/dvr0", dvbv5->scan->adapter );
		player_play ( dvbv5->player, uri );
		free ( uri );

		zap_preview_set_inhibit ( dvbv5 );
	}
	else
	{
		player_stop ( dvbv5->player );
		zap_preview_set_uninhibit ( dvbv5 );

		dvbv5->zap->prw_status = FALSE;
		zap_signal_set_sensitive ( dvbv5->zap );
	}
}

static void zap_signal_toggled_timeov ( GtkToggleButton *button, Dvbv5 *dvbv5 )
{
	// On / Off: Preview -> Time stamp is displayed
	bool status = gtk_toggle_button_get_active ( button );

	if ( dvbv5->player ) player_destroy ( dvbv5->player );
	dvbv5->player = player_new ( status );
}

static bool zap_signal_check_record ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->window == NULL ) return FALSE;

	if ( GST_ELEMENT_CAST ( dvbv5->record->pipeline )->current_state == GST_STATE_NULL )
	{
		zap_set_active_toggled_block ( dvbv5->zap->rec_signal_id, FALSE, dvbv5->zap->toggled_rec );
		return FALSE;
	}

	return TRUE;
}

static void zap_signal_toggled_record ( GtkToggleButton *button, Dvbv5 *dvbv5 )
{
	if ( dvbv5->record == NULL ) { g_critical ( "Record = NULL" ); return; }

	g_timeout_add ( 1000, (GSourceFunc)zap_signal_check_record, dvbv5 );

	if ( !zap_signal_check_all ( dvbv5 ) ) return;

	bool record = gtk_toggle_button_get_active ( button );

	if ( record )
	{
		dvbv5->rec_cnt = 8;
		dvbv5->rec_lr = TRUE;
		dvbv5->zap->rec_status = TRUE;

		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->toggled_prw ), FALSE );
		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->toggled_stm ), FALSE );

		const char *channel = gtk_window_get_title ( dvbv5->window );
		const char *rec_dir = gtk_entry_get_text ( dvbv5->entry_rec_dir );

		char *dtime = zap_signal_time_to_str ();
		char *file = g_strdup_printf ( "/dev/dvb/adapter%d/dvr0", dvbv5->scan->adapter );
		char *file_rec = g_strdup_printf ( "%s/%s-%s.ts", rec_dir, channel, dtime );

		record_set ( dvbv5->record, file, file_rec );
			record_start ( dvbv5->record );

		free ( file );
		free ( dtime );
		free ( file_rec );
	}
	else
	{
		record_stop ( dvbv5->record );
		dvbv5->zap->rec_status = FALSE;
		zap_signal_set_sensitive ( dvbv5->zap );
	}
}

static void zap_signal_trw_row_act ( GtkTreeView *tree_view, GtkTreePath *path, G_GNUC_UNUSED GtkTreeViewColumn *column, Dvbv5 *dvbv5 )
{
	zap_signal_check_fe_lock ( dvbv5 );
	// if ( !zap_signal_check_fe_lock ( dvbv5 ) ) return;

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( GTK_TREE_VIEW ( tree_view ) );

	if ( gtk_tree_model_get_iter ( model, &iter, path ) )
	{
		if ( dvbv5->zap->stm_status || dvbv5->zap->rec_status || dvbv5->zap->prw_status )
		{
			const char *text = "Stop stream.";

			if ( dvbv5->zap->rec_status ) text = "Stop record.";
			if ( dvbv5->zap->prw_status ) text = "Stop preview.";

			dvbv5_message_dialog ( "", text, GTK_MESSAGE_INFO, dvbv5->window );
		}
		else
		{
			char *channel = NULL;
			gtk_tree_model_get ( model, &iter, COL_CHAHHEL, &channel, -1 );

			if ( _dvbv5_dvb_zap_init ( channel, dvbv5 ) )
			{
				gtk_window_set_title ( dvbv5->window, channel );

				if ( !gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON ( dvbv5->toggle_fe ) ) )
					gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( dvbv5->toggle_fe ), TRUE );
			}
			else
				gtk_window_set_title ( dvbv5->window, "Dvbv5-Gtk" );

			free ( channel );
		}
	}
}

bool zap_signal_parse_dvb_file ( const char *file, Dvbv5 *dvbv5 )
{
	if ( file == NULL ) return FALSE;
	if ( !g_file_test ( file, G_FILE_TEST_EXISTS ) ) return FALSE;

	struct dvb_file *dvb_file;
	struct dvb_entry *entry;

	dvb_file = dvb_read_file_format ( file, 0, FILE_DVBV5 ); // dvbv5->cur_sys

	if ( !dvb_file )
	{
		g_critical ( "%s:: Read file format ( only DVBV5 ) failed.", __func__ );
		return FALSE;
	}

	gtk_list_store_clear ( GTK_LIST_STORE ( gtk_tree_view_get_model ( dvbv5->zap->treeview ) ) );

	for ( entry = dvb_file->first_entry; entry != NULL; entry = entry->next )
	{
		if ( entry->channel  ) zap_treeview_append ( entry->channel, entry->service_id,
			( ( entry->audio_pid ) ) ? entry->audio_pid[0] : 0, ( entry->video_pid ) ? entry->video_pid[0] : 0, dvbv5->zap );
		if ( entry->vchannel ) zap_treeview_append ( entry->vchannel, entry->service_id,
			( ( entry->audio_pid ) ) ? entry->audio_pid[0] : 0, ( entry->video_pid ) ? entry->video_pid[0] : 0, dvbv5->zap );
	}

	dvb_file_free ( dvb_file );

	free ( dvbv5->zap_file );
	dvbv5->zap_file = g_strdup ( file );

	if ( dvbv5->zap == NULL ) return TRUE;

	char *name = g_path_get_basename ( file );
		gtk_entry_set_text ( dvbv5->zap->entry_file, name );
	free ( name );

	return TRUE;
}

static void zap_signal_file_open ( GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, Dvbv5 *dvbv5 )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		char *file = file_open ( g_get_home_dir (), entry, dvbv5->window );

		zap_signal_parse_dvb_file ( file, dvbv5 );

		free ( file );
	}
}

static void zap_signal_drag_in ( G_GNUC_UNUSED GtkTreeView *tree_view, GdkDragContext *ct, G_GNUC_UNUSED int x, G_GNUC_UNUSED int y,
        GtkSelectionData *s_data, G_GNUC_UNUSED uint info, guint32 time, Dvbv5 *dvbv5 )
{
	char **uris = gtk_selection_data_get_uris ( s_data );
	char *file = uri_get_path ( uris[0] );

	zap_signal_parse_dvb_file ( file, dvbv5 );

	free ( file );
	g_strfreev ( uris );

	gtk_drag_finish ( ct, TRUE, FALSE, time );
}

void zap_signals ( const OutDemux out_demux_n[], uint8_t n_elm, Dvbv5 *dvbv5 )
{
	uint8_t c = 0; for ( c = 0; c < n_elm; c++ )
		gtk_combo_box_text_append_text ( dvbv5->zap->combo_dmx, out_demux_n[c].name );

	gtk_combo_box_set_active ( GTK_COMBO_BOX ( dvbv5->zap->combo_dmx ), 2 );

	g_signal_connect ( dvbv5->zap->entry_file, "icon-press",         G_CALLBACK ( zap_signal_file_open   ), dvbv5 );
	g_signal_connect ( dvbv5->zap->treeview,   "row-activated",      G_CALLBACK ( zap_signal_trw_row_act ), dvbv5 );
	g_signal_connect ( dvbv5->zap->treeview,   "drag-data-received", G_CALLBACK ( zap_signal_drag_in     ), dvbv5 );

	dvbv5->zap->prw_signal_id = g_signal_connect ( dvbv5->zap->toggled_prw, "toggled", G_CALLBACK ( zap_signal_toggled_preview ), dvbv5 );
	dvbv5->zap->rec_signal_id = g_signal_connect ( dvbv5->zap->toggled_rec, "toggled", G_CALLBACK ( zap_signal_toggled_record  ), dvbv5 );
	dvbv5->zap->stm_signal_id = g_signal_connect ( dvbv5->zap->toggled_stm, "toggled", G_CALLBACK ( zap_signal_toggled_stream  ), dvbv5 );
	dvbv5->zap->tmo_signal_id = g_signal_connect ( dvbv5->zap->toggled_tmo, "toggled", G_CALLBACK ( zap_signal_toggled_timeov  ), dvbv5 );
}
