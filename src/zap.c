/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/gpl-2.0.html
*/

#include "zap.h"
#include "file.h"

#include <linux/dvb/dmx.h>

#include <libdvbv5/dvb-file.h>

enum cols_n
{
	COL_NUM,
	COL_REC,
	COL_PRW,
	COL_CHL,
	COL_SIZE,
	COL_SID,
	COL_VPID,
	COL_APID,
	COL_FILE,
	NUM_COLS
};

typedef struct _OutDemux OutDemux;

struct _OutDemux
{
	uint8_t descr_num;
	const char *name;
};

const OutDemux out_demux_n[] =
{
	{ DMX_OUT_DECODER, 		"DMX_OUT_DECODER" 	  },
	{ DMX_OUT_TAP, 			"DMX_OUT_TAP"		  },
	{ DMX_OUT_TS_TAP, 		"DMX_OUT_TS_TAP" 	  },
	{ DMX_OUT_TSDEMUX_TAP, 	"DMX_OUT_TSDEMUX_TAP" }

};

struct _Zap
{
	GtkBox parent_instance;

	GtkTreeView *treeview;
	GtkEntry *entry_rec;
	GtkEntry *entry_play;
	GtkEntry *entry_file;
	GtkComboBoxText *combo_dmx;

	char *channel;
};

G_DEFINE_TYPE ( Zap, zap, GTK_TYPE_BOX )

static void zap_treeview_append ( const char *channel, uint16_t sid, uint16_t apid, uint16_t vpid, Zap *zap )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( zap->treeview );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );
	if ( ind >= UINT16_MAX ) return;

	gtk_list_store_append ( GTK_LIST_STORE ( model ), &iter );
	gtk_list_store_set    ( GTK_LIST_STORE ( model ), &iter,
				COL_NUM, ind + 1,
				COL_CHL, channel,
				COL_SID, sid,
				COL_VPID, vpid,
				COL_APID, apid,
				-1 );
}

static void zap_signal_trw_act ( GtkTreeView *tree_view, GtkTreePath *path, G_GNUC_UNUSED GtkTreeViewColumn *column, Zap *zap )
{
	uint8_t num_dmx = (uint8_t)gtk_combo_box_get_active ( GTK_COMBO_BOX ( zap->combo_dmx ) );

	uint8_t descr_num = out_demux_n[num_dmx].descr_num; // enum dmx_output

	const char *file = gtk_entry_get_text ( zap->entry_file );

	gboolean fe_lock = FALSE;
	g_signal_emit_by_name ( zap, "zap-get-felock", &fe_lock );

	if ( fe_lock )
	{
		g_message ( "%s:: Zap channel = %s ", __func__, zap->channel );

		g_signal_emit_by_name ( zap, "zap-set-data", descr_num, zap->channel, file );

		return;
	}

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( tree_view );

	if ( !gtk_tree_model_get_iter ( model, &iter, path ) ) return;

	if ( zap->channel ) { free ( zap->channel ); zap->channel = NULL; }
	gtk_tree_model_get ( model, &iter, COL_CHL, &zap->channel, -1 );

	const char *file_rec = gtk_entry_get_text ( zap->entry_rec );

	g_autofree char *date = time_to_str ();
	g_autofree char *dir = g_path_get_dirname ( file_rec );

	char file_new[PATH_MAX];
	sprintf ( file_new, "%s/%s-%s.ts", dir, date, zap->channel );

	gtk_entry_set_text ( zap->entry_rec, file_new );

	g_signal_emit_by_name ( zap, "zap-set-data", descr_num, zap->channel, file );
}

static void zap_treeview_stop_dmx_rec_all_toggled ( Zap *zap )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( zap->treeview );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );
	if ( ind == 0 ) return;

	gboolean valid, active = FALSE, res = FALSE;
	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid;
		  valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		gtk_tree_model_get ( model, &iter, COL_PRW, &active, -1 );

		if ( active ) res = TRUE;

		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_SIZE,   "", -1 );
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_REC, FALSE, -1 );
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PRW, FALSE, -1 );
	}

	if ( res ) sleep ( 1 );
}

static gboolean zap_rec_toggled_size ( GtkTreeRowReference *row )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_row_reference_get_model ( row );
	GtkTreePath *path = gtk_tree_row_reference_get_path ( row );

	gtk_tree_model_get_iter ( model, &iter, path );

	gboolean toggle_item;
	gtk_tree_model_get ( model, &iter, COL_REC, &toggle_item, -1 );

	if ( !toggle_item )
	{
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_SIZE, "", -1 );
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_FILE, "", -1 );

		gtk_tree_row_reference_free ( row );

		return FALSE;
	}
	else
	{
		g_autofree char *file = NULL;
		gtk_tree_model_get ( model, &iter, COL_FILE, &file, -1 );

		uint64_t dsize = file_query_info_uint ( file, "standard::*", "standard::size" );

		g_autofree char *str_size = g_format_size ( dsize );

		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_SIZE, str_size, -1 );
	}

	return TRUE;
}

static void zap_rec_toggled ( G_GNUC_UNUSED GtkCellRendererToggle *toggle, char *path_str, Zap *zap )
{
	gboolean fe_lock = FALSE;
	g_signal_emit_by_name ( zap, "zap-get-felock", &fe_lock );

	if ( !fe_lock )
	{
		GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( zap->treeview ) ) );

		dvb5_message_dialog ( "", "Zap?", GTK_MESSAGE_WARNING, window );

		g_message ( "%s:: Zap? ", __func__ );

		return;
	}

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( zap->treeview );

	GtkTreePath *path = gtk_tree_path_new_from_string ( path_str );
	gtk_tree_model_get_iter ( model, &iter, path );

	gboolean toggle_item;
	gtk_tree_model_get ( model, &iter, COL_REC, &toggle_item, -1 );

	toggle_item = !toggle_item;
	gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_SIZE, "", -1 );
	gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_FILE, "", -1 );

	gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_REC, toggle_item, -1 );

	if ( toggle_item )
	{
		g_autofree char *channel = NULL;
		gtk_tree_model_get ( model, &iter, COL_CHL, &channel, -1 );

		uint sid = 0, vpid = 0, apid = 0;
		gtk_tree_model_get ( model, &iter, COL_SID,  &sid,  -1 );
		gtk_tree_model_get ( model, &iter, COL_VPID, &vpid, -1 );
		gtk_tree_model_get ( model, &iter, COL_APID, &apid, -1 );

		uint16_t pids[] = { (uint16_t)sid, (uint16_t)vpid, (uint16_t)apid };

		const char *file_rec = gtk_entry_get_text ( zap->entry_rec );

		g_autofree char *date = time_to_str ();
		g_autofree char *dir = g_path_get_dirname ( file_rec );

		char file_new[PATH_MAX];
		sprintf ( file_new, "%s/%s-%s.ts", dir, date, channel );

		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_FILE, file_new, -1 );

		uint8_t adapter = 0;
		g_signal_emit_by_name ( zap, "zap-get-adapter", &adapter );

		uint8_t demux = 0;
		g_signal_emit_by_name ( zap, "zap-get-demux", &demux );

		const char *res = dmx_rec_create ( adapter, demux, file_new, 3, pids, model, iter );

		if ( res )
		{
			GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( zap->treeview ) ) );
			dvb5_message_dialog ( "", res, GTK_MESSAGE_WARNING, window );
		}
		else
		{	
			GtkTreeRowReference *row = gtk_tree_row_reference_new ( model, path );

			g_timeout_add ( 1000, (GSourceFunc)zap_rec_toggled_size, row );
		}

		g_message ( "%s:: toggle active %s, %s ", __func__, ( toggle_item ) ? "True" : "False", channel );
	}

	gtk_tree_path_free ( path );
}

static void zap_prw_toggled ( G_GNUC_UNUSED GtkCellRendererToggle *toggle, char *path_str, Zap *zap )
{
	gboolean fe_lock = FALSE;
	g_signal_emit_by_name ( zap, "zap-get-felock", &fe_lock );

	if ( !fe_lock )
	{
		GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( zap->treeview ) ) );

		dvb5_message_dialog ( "", "Zap?", GTK_MESSAGE_WARNING, window );

		g_message ( "%s:: Zap? ", __func__ );

		return;
	}

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( zap->treeview );

	GtkTreePath *path = gtk_tree_path_new_from_string ( path_str );
	gtk_tree_model_get_iter ( model, &iter, path );

	gboolean toggle_item;
	gtk_tree_model_get ( model, &iter, COL_PRW, &toggle_item, -1 );

	toggle_item = !toggle_item;
	gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PRW, toggle_item, -1 );

	if ( toggle_item )
	{
		g_autofree char *channel = NULL;
		gtk_tree_model_get ( model, &iter, COL_CHL, &channel, -1 );

		uint sid = 0, vpid = 0, apid = 0;
		gtk_tree_model_get ( model, &iter, COL_SID,  &sid,  -1 );
		gtk_tree_model_get ( model, &iter, COL_VPID, &vpid, -1 );
		gtk_tree_model_get ( model, &iter, COL_APID, &apid, -1 );

		uint16_t pids[] = { (uint16_t)sid, (uint16_t)vpid, (uint16_t)apid };

		g_autofree char *date = time_to_str ();

		char file_new[PATH_MAX];
		sprintf ( file_new, "/tmp/%s", date );

		uint8_t adapter = 0;
		g_signal_emit_by_name ( zap, "zap-get-adapter", &adapter );

		uint8_t demux = 0;
		g_signal_emit_by_name ( zap, "zap-get-demux", &demux );

		const char *player = gtk_entry_get_text ( zap->entry_play );
		const char *res = dmx_prw_create ( adapter, demux, file_new, 3, pids, model, iter, player );

		if ( res )
		{
			GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( zap->treeview ) ) );
			dvb5_message_dialog ( "", res, GTK_MESSAGE_WARNING, window );
		}

		g_message ( "%s:: toggle active %s, %s ", __func__, ( toggle_item ) ? "True" : "False", channel );
	}

	gtk_tree_path_free ( path );
}

static GtkScrolledWindow * zap_create_treeview_scroll ( Zap *zap )
{
	GtkScrolledWindow *scroll = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_visible ( GTK_WIDGET ( scroll ), TRUE );

	GtkListStore *store = gtk_list_store_new ( NUM_COLS, G_TYPE_UINT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_STRING, 
		G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING );

	zap->treeview = (GtkTreeView *)gtk_tree_view_new_with_model ( GTK_TREE_MODEL ( store ) );
	gtk_drag_dest_set ( GTK_WIDGET ( zap->treeview ), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY );
	gtk_drag_dest_add_uri_targets  ( GTK_WIDGET ( zap->treeview ) );
	gtk_widget_set_visible ( GTK_WIDGET ( zap->treeview ), TRUE );

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	struct Column { const char *name; const char *type; uint8_t num; } column_n[] =
	{
		{ "Num",        "text",   COL_NUM  },
		{ "Rec",        "active", COL_REC  },
		{ "Prw",        "active", COL_PRW  },
		{ "Channel",    "text",   COL_CHL  },
		{ "Size",       "text",   COL_SIZE },
		{ "SID",      	"text",   COL_SID  },
		{ "Video",      "text",   COL_VPID },
		{ "Audio",      "text",   COL_APID },
		{ "File",       "text",   COL_FILE }
	};

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( column_n ); c++ )
	{
		if ( c == COL_REC || c == COL_PRW )
		{
			renderer = gtk_cell_renderer_toggle_new ();
			if ( c == COL_REC ) g_signal_connect ( renderer, "toggled", G_CALLBACK ( zap_rec_toggled ), zap );
			if ( c == COL_PRW ) g_signal_connect ( renderer, "toggled", G_CALLBACK ( zap_prw_toggled ), zap );
		}
		else
			renderer = gtk_cell_renderer_text_new ();

		column = gtk_tree_view_column_new_with_attributes ( column_n[c].name, renderer, column_n[c].type, column_n[c].num, NULL );

		if ( c == COL_SID || c == COL_VPID || c == COL_APID || c == COL_FILE ) 
			gtk_tree_view_column_set_visible ( column, FALSE );

		gtk_tree_view_append_column ( zap->treeview, column );
	}

	gtk_container_add ( GTK_CONTAINER ( scroll ), GTK_WIDGET ( zap->treeview ) );
	g_object_unref ( G_OBJECT (store) );

	return scroll;
}

static void zap_combo_dmx_add ( uint8_t n_elm, Zap *zap )
{
	uint8_t c = 0; for ( c = 0; c < n_elm; c++ )
		gtk_combo_box_text_append_text ( zap->combo_dmx, out_demux_n[c].name );

	gtk_combo_box_set_active ( GTK_COMBO_BOX ( zap->combo_dmx ), 2 );
}

static gboolean zap_signal_parse_dvb_file ( const char *file, Zap *zap )
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

	gtk_list_store_clear ( GTK_LIST_STORE ( gtk_tree_view_get_model ( zap->treeview ) ) );

	for ( entry = dvb_file->first_entry; entry != NULL; entry = entry->next )
	{
		if ( entry->channel  ) zap_treeview_append ( entry->channel, entry->service_id, 
			( entry->audio_pid ) ? entry->audio_pid[0] : 0, ( entry->video_pid ) ? entry->video_pid[0] : 0, zap );

		if ( entry->vchannel ) zap_treeview_append ( entry->vchannel, entry->service_id, 
			( entry->audio_pid ) ? entry->audio_pid[0] : 0, ( entry->video_pid ) ? entry->video_pid[0] : 0, zap );
	}

	dvb_file_free ( dvb_file );

	gtk_entry_set_text ( zap->entry_file, file );

	return TRUE;
}

static void zap_signal_file_open ( GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, Zap *zap )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( entry ) ) );

		g_autofree char *file = file_open ( g_get_home_dir (), window );

		zap_signal_parse_dvb_file ( file, zap );
	}
}

static void zap_signal_drag_in ( G_GNUC_UNUSED GtkTreeView *tree_view, GdkDragContext *ct, G_GNUC_UNUSED int x, G_GNUC_UNUSED int y,
        GtkSelectionData *s_data, G_GNUC_UNUSED uint info, guint32 time, Zap *zap )
{
	char **uris = gtk_selection_data_get_uris ( s_data );

	g_autofree char *file = uri_get_path ( uris[0] );

	zap_signal_parse_dvb_file ( file, zap );

	g_strfreev ( uris );

	gtk_drag_finish ( ct, TRUE, FALSE, time );
}

static void zap_clicked_clear ( G_GNUC_UNUSED GtkButton *button, Zap *zap )
{
	gtk_entry_set_text ( zap->entry_file, "" );
	gtk_list_store_clear ( GTK_LIST_STORE ( gtk_tree_view_get_model ( zap->treeview ) ) );
}

static void zap_signal_record_file ( GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, G_GNUC_UNUSED Zap *zap )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( entry ) ) );

		const char *file_rec = gtk_entry_get_text ( zap->entry_rec );

		g_autofree char *name = g_path_get_basename ( file_rec );

		g_autofree char *file = file_save ( g_get_home_dir (), name, window );

		if ( file ) gtk_entry_set_text ( entry, file );
	}
}

static void zap_handler_stop ( Zap *zap )
{
	zap_treeview_stop_dmx_rec_all_toggled ( zap );

	if ( zap->channel ) { free ( zap->channel ); zap->channel = NULL; }
}

static GtkBox * zap_set_record_file ( const char *file, Zap *zap )
{
	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	zap->entry_rec = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( zap->entry_rec, file );
	gtk_widget_set_size_request ( GTK_WIDGET ( zap->entry_rec ), 200, -1 );
	g_object_set ( zap->entry_rec, "editable", FALSE, NULL );
	gtk_entry_set_icon_from_icon_name ( zap->entry_rec, GTK_ENTRY_ICON_SECONDARY, "folder" );

	g_signal_connect ( zap->entry_rec, "icon-press", G_CALLBACK ( zap_signal_record_file ), zap );

	zap->entry_play = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( zap->entry_play, "mplayer -nocache" ); // ffplay | mpv | mplayer -nocache

	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->entry_rec  ), TRUE, TRUE, 0 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->entry_play ), TRUE, TRUE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( zap->entry_rec  ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( zap->entry_play ), TRUE );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	return v_box;
}

static void zap_init ( Zap *zap )
{
	GtkBox *box = GTK_BOX ( zap );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( box ), GTK_ORIENTATION_VERTICAL );
	gtk_box_set_spacing ( box, 10 );
	gtk_widget_set_visible ( GTK_WIDGET ( box ), TRUE );

	gtk_widget_set_margin_top    ( GTK_WIDGET ( box ), 10 );
	gtk_widget_set_margin_bottom ( GTK_WIDGET ( box ), 10 );
	gtk_widget_set_margin_start  ( GTK_WIDGET ( box ), 10 );
	gtk_widget_set_margin_end    ( GTK_WIDGET ( box ), 10 );

	gtk_box_pack_start ( box, GTK_WIDGET ( zap_create_treeview_scroll ( zap ) ), TRUE, TRUE, 0 );

	g_signal_connect ( zap->treeview, "drag-data-received", G_CALLBACK ( zap_signal_drag_in ), zap );
	g_signal_connect ( zap->treeview, "row-activated",      G_CALLBACK ( zap_signal_trw_act ), zap );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkButton *button_clear = (GtkButton *)gtk_button_new_from_icon_name ( "dvb-clear", GTK_ICON_SIZE_MENU );
	gtk_widget_set_visible ( GTK_WIDGET ( button_clear ), TRUE );

	zap->entry_file = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( zap->entry_file, "dvb_channel.conf" );
	gtk_widget_set_size_request ( GTK_WIDGET ( zap->entry_file ), 240, -1 );
	g_object_set ( zap->entry_file, "editable", FALSE, NULL );
	gtk_entry_set_icon_from_icon_name ( zap->entry_file, GTK_ENTRY_ICON_SECONDARY, "folder" );
	g_signal_connect ( zap->entry_file, "icon-press", G_CALLBACK ( zap_signal_file_open ), zap );

	g_signal_connect ( button_clear, "clicked", G_CALLBACK ( zap_clicked_clear ), zap );

	const char *icon = "dvb-info";
	gtk_entry_set_icon_from_icon_name ( zap->entry_file, GTK_ENTRY_ICON_PRIMARY, icon );
	gtk_entry_set_icon_tooltip_text ( GTK_ENTRY ( zap->entry_file ), GTK_ENTRY_ICON_PRIMARY, "Format only DVBV5" );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->entry_file ), TRUE, TRUE, 0 );

	zap->combo_dmx = (GtkComboBoxText *) gtk_combo_box_text_new ();
	zap_combo_dmx_add ( G_N_ELEMENTS ( out_demux_n ), zap );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->combo_dmx ), TRUE, TRUE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( zap->entry_file ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( zap->combo_dmx  ), TRUE );

	gtk_box_pack_end ( h_box, GTK_WIDGET ( button_clear ), FALSE, FALSE, 0 );

	char file_rec[PATH_MAX];
	sprintf ( file_rec, "%s/%s.ts", g_get_home_dir (), "Record" );

	GtkBox *box_rec = zap_set_record_file ( file_rec, zap );

	gtk_box_pack_start ( box, GTK_WIDGET ( box_rec ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	zap->channel = NULL;
	g_signal_connect ( zap, "zap-stop", G_CALLBACK ( zap_handler_stop ), NULL );
}

static void zap_finalize ( GObject *object )
{
	Zap *zap = ZAP_BOX ( object );

	if ( zap->channel ) free ( zap->channel );

	G_OBJECT_CLASS (zap_parent_class)->finalize (object);
}

static void zap_class_init ( ZapClass *class )
{
	G_OBJECT_CLASS (class)->finalize = zap_finalize;

	g_signal_new ( "zap-get-felock", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_BOOLEAN, 0 );

	g_signal_new ( "zap-get-adapter", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_UINT, 0 );

	g_signal_new ( "zap-get-demux", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_UINT, 0 );

	g_signal_new ( "zap-stop", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 0 );

	g_signal_new ( "zap-set-data", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING );
}

Zap * zap_new (void)
{
	return g_object_new ( ZAP_TYPE_BOX, NULL );
}

