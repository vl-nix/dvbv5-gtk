/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "dvbv5.h"
#include "zap.h"
#include "file.h"
#include "dmx-rec-prw.h"

/* Returns a newly-allocated string holding the result. Free with free() */
static char * zap_signal_time_to_str (void)
{
	GDateTime *date = g_date_time_new_now_local ();

	char *str_time = g_date_time_format ( date, "%j-%Y-%T" );

	g_date_time_unref ( date );

	return str_time;
}

#ifndef LIGHT

static gboolean zap_signal_check_fe_lock ( Dvbv5 *dvbv5 )
{
	if ( !dvbv5->dvb->fe_lock )
	{
		// g_signal_emit_by_name ( dvbv5->control, "button-clicked", "stop" );
		dvbv5_message_dialog ( "Zap:", "FE_HAS_LOCK - failed", GTK_MESSAGE_WARNING, dvbv5->window );
		return FALSE;
	}

	return TRUE;
}

static gboolean zap_signal_check_dmx_out ( Dvbv5 *dvbv5 )
{
	uint8_t num_dmx = (uint8_t)gtk_combo_box_get_active ( GTK_COMBO_BOX ( dvbv5->zap->combo_dmx ) );
	uint8_t descr_num = zap_get_dmx ( num_dmx );

	if ( descr_num == DMX_OUT_TS_TAP || descr_num == DMX_OUT_ALL_PIDS ) return TRUE;

	dvbv5_message_dialog ( "", "Support: DMX_OUT_TS_TAP or DMX_OUT_ALL_PIDS", GTK_MESSAGE_INFO, dvbv5->window );

	return FALSE;
}

static void zap_signal_set_sensitive ( Zap *zap)
{
	gtk_widget_set_sensitive ( GTK_WIDGET ( zap->toggled_prw ), TRUE );
	gtk_widget_set_sensitive ( GTK_WIDGET ( zap->toggled_rec ), TRUE );
	gtk_widget_set_sensitive ( GTK_WIDGET ( zap->toggled_stm ), TRUE );
}

static gboolean zap_signal_check_all ( Dvbv5 *dvbv5 )
{
	if ( !dvbv5->dvb->dvb_zap ) { dvbv5_message_dialog ( "", "Zap?", GTK_MESSAGE_WARNING, dvbv5->window ); return FALSE; }
	if ( !zap_signal_check_fe_lock ( dvbv5 ) ) return FALSE;
	if ( !zap_signal_check_dmx_out ( dvbv5 ) ) return FALSE;

	return TRUE;
}

static gboolean zap_signal_check_stream ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->window == NULL ) return FALSE;

	if ( GST_ELEMENT_CAST ( dvbv5->server->pipeline )->current_state == GST_STATE_NULL )
	{
		dvbv5->zap->stm_status = FALSE;
		zap_signal_set_sensitive ( dvbv5->zap );
		zap_set_active_toggled_block ( dvbv5->zap->stm_signal_id, FALSE, dvbv5->zap->toggled_stm );
		return FALSE;
	}

	return TRUE;
}

static void zap_dvr_stream_start ( G_GNUC_UNUSED GtkButton *button, Dvbv5 *dvbv5 )
{
	char dvrdev[PATH_MAX];
	sprintf ( dvrdev, "/dev/dvb/adapter%d/dvr0", dvbv5->dvb->adapter );

	tcpserver_set ( dvbv5->server, dvbv5->zap->host, dvbv5->zap->port, dvrdev );
	tcpserver_start ( dvbv5->server );

	g_timeout_add ( 1000, (GSourceFunc)zap_signal_check_stream, dvbv5 );
}

static void zap_dvr_stream_entry_changed_host ( GtkEntry *entry, Dvbv5 *dvbv5 )
{
	const char *text = gtk_entry_get_text ( entry );

	free ( dvbv5->zap->host );
	dvbv5->zap->host = g_strdup ( text );
}

static void zap_dvr_stream_entry_changed_port ( GtkEntry *entry, Dvbv5 *dvbv5 )
{
	const char *text = gtk_entry_get_text ( entry );

	dvbv5->zap->port = (uint16_t)atoi ( text );
}

static void zap_dvr_stream_window_quit ( G_GNUC_UNUSED GtkWindow *window, Dvbv5 *dvbv5 )
{
	g_timeout_add ( 1000, (GSourceFunc)zap_signal_check_stream, dvbv5 );
}

static void zap_dvr_stream_set_host ( Dvbv5 *dvbv5 )
{
	GtkWindow *window = (GtkWindow *)gtk_window_new ( GTK_WINDOW_TOPLEVEL );

	gtk_window_set_title ( window, "Server - Dvr" );
	gtk_window_set_modal ( window, TRUE );
	gtk_window_set_default_size ( window, 400, 100 );
	g_signal_connect ( window, "destroy", G_CALLBACK ( zap_dvr_stream_window_quit ), dvbv5 );

	GtkBox *m_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_set_spacing ( m_box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( m_box ), TRUE );

	char port[10];
	sprintf ( port, "%d", dvbv5->zap->port );

	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( entry, dvbv5->zap->host );
	g_signal_connect ( entry, "changed", G_CALLBACK ( zap_dvr_stream_entry_changed_host ), dvbv5 );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( entry ), FALSE, FALSE, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( entry ), TRUE );

	entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( entry, port );
	g_signal_connect ( entry, "changed", G_CALLBACK ( zap_dvr_stream_entry_changed_port ), dvbv5 );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( entry ), FALSE, FALSE, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( entry ), TRUE );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkButton *button = control_create_button ( NULL, "dvb-quit", "⏻", 16 );
	g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( button ), TRUE, TRUE, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );

	button = control_create_button ( NULL, "dvb-start", "⏵", 16 );
	g_signal_connect ( button, "clicked", G_CALLBACK ( zap_dvr_stream_start ), dvbv5 );
	g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( button ), TRUE, TRUE, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );

	gtk_box_pack_end ( m_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( m_box ), 10 );
	gtk_container_add ( GTK_CONTAINER ( window ), GTK_WIDGET ( m_box ) );

	gtk_window_present ( window );
}

static void zap_signal_toggled_stream ( GtkToggleButton *button, Dvbv5 *dvbv5 )
{
	if ( dvbv5->server == NULL ) { g_critical ( "Server = NULL" ); return; }

	if ( !zap_signal_check_all ( dvbv5 ) ) { g_timeout_add ( 1000, (GSourceFunc)zap_signal_check_stream, dvbv5 ); return; }

	gboolean server = gtk_toggle_button_get_active ( button );

	if ( server )
	{
		dvbv5->zap->stm_status = TRUE;

		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->toggled_prw ), FALSE );
		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->toggled_rec ), FALSE );

		zap_dvr_stream_set_host ( dvbv5 );
	}
	else
	{
		tcpserver_stop ( dvbv5->server );
		dvbv5->zap->stm_status = FALSE;
		zap_signal_set_sensitive ( dvbv5->zap );
	}
}

static gboolean zap_signal_check_preview ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->window == NULL ) return FALSE;

	if ( GST_ELEMENT_CAST ( dvbv5->player->playbin )->current_state == GST_STATE_NULL )
	{
		dvbv5->zap->prw_status = FALSE;
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

	gboolean preview = gtk_toggle_button_get_active ( button );

	if ( preview )
	{
		dvbv5->zap->prw_status = TRUE;

		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->toggled_rec ), FALSE );
		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->toggled_stm ), FALSE );

		char uri[PATH_MAX];
		sprintf ( uri, "file:///dev/dvb/adapter%d/dvr0", dvbv5->dvb->adapter );

		player_play ( dvbv5->player, uri );
	}
	else
	{
		player_stop ( dvbv5->player );
		dvbv5->zap->prw_status = FALSE;
		zap_signal_set_sensitive ( dvbv5->zap );
	}
}

static gboolean zap_signal_check_record ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->window == NULL ) return FALSE;

	if ( GST_ELEMENT_CAST ( dvbv5->record->pipeline )->current_state == GST_STATE_NULL )
	{
		dvbv5->zap->rec_status = FALSE;
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

	gboolean record = gtk_toggle_button_get_active ( button );

	if ( record )
	{
		dvbv5->zap->rec_status = TRUE;

		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->toggled_prw ), FALSE );
		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->toggled_stm ), FALSE );

		const char *channel = gtk_window_get_title ( dvbv5->window );

		g_autofree char *dtime = zap_signal_time_to_str ();
		g_autofree char *s_dir = settings_get_s ( "rec-dir", dvbv5->setting );

		char dvrdev[PATH_MAX];
		sprintf ( dvrdev, "/dev/dvb/adapter%d/dvr0", dvbv5->dvb->adapter );

		char file_rec[PATH_MAX];
		sprintf ( file_rec, "%s/%s-%s.ts", ( s_dir ) ? s_dir : g_get_home_dir (), channel, dtime );

		record_set   ( dvbv5->record, dvrdev, file_rec );
		record_start ( dvbv5->record );
	}
	else
	{
		record_stop ( dvbv5->record );
		dvbv5->zap->rec_status = FALSE;
		zap_signal_set_sensitive ( dvbv5->zap );
	}
}

#endif

static gboolean zap_signal_check_toggle_fe ( Dvbv5 *dvbv5 )
{
	if ( !gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON ( dvbv5->toggle_fe ) ) )
		gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( dvbv5->toggle_fe ), TRUE );

	return FALSE;
}

static void zap_signal_trw_row_act ( GtkTreeView *tree_view, GtkTreePath *path, G_GNUC_UNUSED GtkTreeViewColumn *column, Dvbv5 *dvbv5 )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( tree_view );

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
			if ( !g_file_test ( dvbv5->dvb->zap_file, G_FILE_TEST_EXISTS ) )
			{
				dvbv5_message_dialog ( dvbv5->dvb->zap_file, g_strerror ( errno ), GTK_MESSAGE_ERROR, dvbv5->window );
				return;
			}

			g_signal_emit_by_name ( dvbv5->control, "button-clicked", "stop" );

			g_autofree char *channel = NULL;
			gtk_tree_model_get ( model, &iter, COL_CHAHHEL, &channel, -1 );

			uint8_t num_dmx = (uint8_t)gtk_combo_box_get_active ( GTK_COMBO_BOX ( dvbv5->zap->combo_dmx ) );
			uint8_t descr_num = zap_get_dmx ( num_dmx );

			const char *error = dvb_zap ( channel, descr_num, dvbv5->dvb );

			if ( error )
			{
				gtk_window_set_title ( dvbv5->window, PROGNAME );
				dvbv5_message_dialog ( "", error, GTK_MESSAGE_ERROR, dvbv5->window );
			}
			else
			{
				dvbv5->zap->zap_iter = iter;
				gtk_window_set_title ( dvbv5->window, channel );
				g_timeout_add ( 250, (GSourceFunc)zap_signal_check_toggle_fe, dvbv5 );
#ifndef LIGHT
				dvbv5_dmx_monitor_clear ( dvbv5 );
#endif
			}
		}
	}
}

gboolean zap_signal_parse_dvb_file ( const char *file, Dvbv5 *dvbv5 )
{
	if ( file == NULL ) return FALSE;
	if ( !g_file_test ( file, G_FILE_TEST_EXISTS ) ) return FALSE;

	struct dvb_file *dvb_file;
	struct dvb_entry *entry;

	dvb_file = dvb_read_file_format ( file, 0, FILE_DVBV5 );

	if ( !dvb_file )
	{
		g_critical ( "%s:: Read file format ( only DVBV5 ) failed.", __func__ );
		return FALSE;
	}

	g_signal_emit_by_name ( dvbv5->control, "button-clicked", "stop" );
	gtk_list_store_clear ( GTK_LIST_STORE ( gtk_tree_view_get_model ( dvbv5->zap->treeview ) ) );

	for ( entry = dvb_file->first_entry; entry != NULL; entry = entry->next )
	{
		uint32_t f = 0, delsys = SYS_UNDEFINED;;
		dvb_retrieve_entry_prop ( entry, DTV_FREQUENCY, &f );
		dvb_retrieve_entry_prop ( entry, DTV_DELIVERY_SYSTEM, &delsys );

		uint8_t is_sat = dvb_fe_get_is_satellite ( (uint8_t)delsys );
		f /= ( is_sat ) ? 1000 : 1000000;

		if ( entry->channel  ) zap_treeview_append ( entry->channel, entry->service_id,
			( ( entry->audio_pid ) ) ? entry->audio_pid[0] : 0, ( entry->video_pid ) ? entry->video_pid[0] : 0, f, dvbv5->zap );

		if ( entry->vchannel ) zap_treeview_append ( entry->vchannel, entry->service_id,
			( ( entry->audio_pid ) ) ? entry->audio_pid[0] : 0, ( entry->video_pid ) ? entry->video_pid[0] : 0, f, dvbv5->zap );
	}

	dvb_file_free ( dvb_file );

	free ( dvbv5->dvb->zap_file );
	dvbv5->dvb->zap_file = g_strdup ( file );

#ifndef LIGHT
	settings_set_s ( file, "zap-conf", dvbv5->setting );
#endif

	if ( dvbv5->zap == NULL ) return TRUE;

	g_autofree char *name = g_path_get_basename ( file );

	gtk_entry_set_text ( dvbv5->zap->entry_file, name );

	return TRUE;
}

static void zap_signal_file_open ( GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, Dvbv5 *dvbv5 )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		g_autofree char *file = file_open ( g_get_home_dir (), entry, dvbv5->window );

		zap_signal_parse_dvb_file ( file, dvbv5 );
	}
}

static void zap_signal_drag_in ( G_GNUC_UNUSED GtkTreeView *tree_view, GdkDragContext *ct, G_GNUC_UNUSED int x, G_GNUC_UNUSED int y,
        GtkSelectionData *s_data, G_GNUC_UNUSED uint info, guint32 time, Dvbv5 *dvbv5 )
{
	char **uris = gtk_selection_data_get_uris ( s_data );

	g_autofree char *file = uri_get_path ( uris[0] );

	zap_signal_parse_dvb_file ( file, dvbv5 );

	g_strfreev ( uris );

	gtk_drag_finish ( ct, TRUE, FALSE, time );
}

static gboolean zap_signal_check_all_dmx ( Dvbv5 *dvbv5 )
{
	if ( !dvbv5->dvb->dvb_zap ) { dvbv5_message_dialog ( "", "Zap?", GTK_MESSAGE_WARNING, dvbv5->window ); return FALSE; }
	if ( !dvbv5->dvb->fe_lock ) { dvbv5_message_dialog ( "Zap:", "FE_HAS_LOCK - failed", GTK_MESSAGE_WARNING, dvbv5->window ); return FALSE; }

	return TRUE;
}

#ifndef LIGHT
static void zap_signal_toggled_dmx_prw ( Zap *zap, const char *path_str, Dvbv5 *dvbv5 )
{
	if ( !zap_signal_check_all_dmx ( dvbv5 ) ) return;

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( zap->treeview );

	GtkTreePath *path = gtk_tree_path_new_from_string ( path_str );
	gtk_tree_model_get_iter ( model, &iter, path );

	gboolean toggle_item;
	gtk_tree_model_get ( model, &iter, COL_PRW, &toggle_item, -1 );

	toggle_item = !toggle_item;
	gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PRW, toggle_item, -1 );

	uint num = 0;
	gtk_tree_model_get ( model, &iter, COL_NUM,  &num,  -1 );

	if ( toggle_item )
	{
		uint sid = 0, vpid = 0, apid = 0;
		gtk_tree_model_get ( model, &iter, COL_SID,  &sid,  -1 );
		gtk_tree_model_get ( model, &iter, COL_VPID, &vpid, -1 );
		gtk_tree_model_get ( model, &iter, COL_APID, &apid, -1 );

		uint16_t pids[] = { 0, (uint16_t)sid, (uint16_t)vpid, (uint16_t)apid, 4 };

		dmx_prw_create ( dvbv5->dvb->adapter, dvbv5->dvb->demux, (uint16_t)num, pids, zap->monitor );
	}
	else
	{
		dvbv5_dmx_monitor_stop_one ( (uint16_t)num, TRUE, FALSE, dvbv5 );
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PRW_BITR, 0, -1 );
	}

	gtk_tree_path_free ( path );
}
#endif

static void zap_signal_toggled_dmx_rec ( Zap *zap, const char *path_str, Dvbv5 *dvbv5 )
{
	if ( !zap_signal_check_all_dmx ( dvbv5 ) ) return;

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( zap->treeview );

	GtkTreePath *path = gtk_tree_path_new_from_string ( path_str );
	gtk_tree_model_get_iter ( model, &iter, path );

	gboolean toggle_item;
	gtk_tree_model_get ( model, &iter, COL_REC, &toggle_item, -1 );

	toggle_item = !toggle_item;
	gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_REC, toggle_item, -1 );
	gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_REC_SIZE, "", -1 );
	gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_REC_FILE, "", -1 );

	uint num = 0;
	gtk_tree_model_get ( model, &iter, COL_NUM,  &num,  -1 );

	if ( toggle_item )
	{
		g_autofree char *dtime = zap_signal_time_to_str ();

		g_autofree char *channel = NULL;
		gtk_tree_model_get ( model, &iter, COL_CHAHHEL, &channel, -1 );

		uint sid = 0, vpid = 0, apid = 0;
		gtk_tree_model_get ( model, &iter, COL_SID,  &sid,  -1 );
		gtk_tree_model_get ( model, &iter, COL_VPID, &vpid, -1 );
		gtk_tree_model_get ( model, &iter, COL_APID, &apid, -1 );

		uint16_t pids[] = { 0, (uint16_t)sid, (uint16_t)vpid, (uint16_t)apid, 4 };

		char dmxrec[PATH_MAX];

#ifndef LIGHT
		g_autofree char *s_dir = settings_get_s ( "rec-dir", dvbv5->setting );
		sprintf ( dmxrec, "%s/%s-%s", s_dir, channel, dtime );
#else
		sprintf ( dmxrec, "%s/%s-%s", g_get_home_dir (), channel, dtime );
#endif

		dmx_rec_create ( dvbv5->dvb->adapter, dvbv5->dvb->demux, (uint16_t)num, dmxrec, pids, zap->monitor );

		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_REC_FILE, dmxrec, -1 );
	}
#ifndef LIGHT
	else
	{
		dvbv5_dmx_monitor_stop_one ( (uint16_t)num, FALSE, TRUE, dvbv5 );
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PRW_BITR, 0, -1 );
	}
#endif
	gtk_tree_path_free ( path );
}

void zap_signals ( Dvbv5 *dvbv5 )
{
#ifndef LIGHT
	g_signal_connect ( dvbv5->zap, "button-toggled-prw", G_CALLBACK ( zap_signal_toggled_dmx_prw ), dvbv5 );
#endif
	g_signal_connect ( dvbv5->zap, "button-toggled-rec", G_CALLBACK ( zap_signal_toggled_dmx_rec ), dvbv5 );

	g_signal_connect ( dvbv5->zap->entry_file, "icon-press",         G_CALLBACK ( zap_signal_file_open   ), dvbv5 );
	g_signal_connect ( dvbv5->zap->treeview,   "row-activated",      G_CALLBACK ( zap_signal_trw_row_act ), dvbv5 );
	g_signal_connect ( dvbv5->zap->treeview,   "drag-data-received", G_CALLBACK ( zap_signal_drag_in     ), dvbv5 );

#ifndef LIGHT
	dvbv5->zap->prw_signal_id = g_signal_connect ( dvbv5->zap->toggled_prw, "toggled", G_CALLBACK ( zap_signal_toggled_preview ), dvbv5 );
	dvbv5->zap->rec_signal_id = g_signal_connect ( dvbv5->zap->toggled_rec, "toggled", G_CALLBACK ( zap_signal_toggled_record  ), dvbv5 );
	dvbv5->zap->stm_signal_id = g_signal_connect ( dvbv5->zap->toggled_stm, "toggled", G_CALLBACK ( zap_signal_toggled_stream  ), dvbv5 );
#endif
}
