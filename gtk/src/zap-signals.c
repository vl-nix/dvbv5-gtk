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

static void zap_signal_trw_row_act ( GtkTreeView *tree_view, GtkTreePath *path, G_GNUC_UNUSED GtkTreeViewColumn *column, Dvbv5 *dvbv5 )
{
	if ( !zap_signal_check_fe_lock ( dvbv5 ) ) return;

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( GTK_TREE_VIEW ( tree_view ) );

	if ( gtk_tree_model_get_iter ( model, &iter, path ) )
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
				gtk_window_set_title ( dvbv5->window, "Dvbv5-Light-Gtk" );

			free ( channel );
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

void zap_signals ( const OutDemux out_demux_n[], uint8_t n_elm, Dvbv5 *dvbv5 )
{
	uint8_t c = 0; for ( c = 0; c < n_elm; c++ )
		gtk_combo_box_text_append_text ( dvbv5->zap->combo_dmx, out_demux_n[c].name );

	gtk_combo_box_set_active ( GTK_COMBO_BOX ( dvbv5->zap->combo_dmx ), 2 );

	g_signal_connect ( dvbv5->zap->entry_file, "icon-press",         G_CALLBACK ( zap_signal_file_open   ), dvbv5 );
	g_signal_connect ( dvbv5->zap->treeview,   "row-activated",      G_CALLBACK ( zap_signal_trw_row_act ), dvbv5 );
}
