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

#include <glib/gi18n.h>

#include <linux/dvb/dmx.h>

#include <libdvbv5/dvb-file.h>

enum cols_n
{
	COL_NUM,
	COL_CHL,
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
	{ DMX_OUT_DECODER, 	"DMX_OUT_DECODER" 	},
	{ DMX_OUT_TAP, 		"DMX_OUT_TAP"		},
	{ DMX_OUT_TS_TAP, 	"DMX_OUT_TS_TAP" 	},
	{ DMX_OUT_TSDEMUX_TAP, 	"DMX_OUT_TSDEMUX_TAP" 	}

};

struct _Zap
{
	GtkBox parent_instance;

	GtkTreeView *treeview;
	GtkEntry *entry_rec;
	GtkEntry *entry_play;
	GtkEntry *entry_file;
	GtkComboBoxText *combo_dmx;
	GtkCheckButton *check_rec;
	GtkCheckButton *check_play;

	DwrRecMonitor *dm;

	char *channel;

	int pid;

	ulong rec_signal_id;
	ulong play_signal_id;

	gboolean exit;
};

G_DEFINE_TYPE ( Zap, zap, GTK_TYPE_BOX )

static gboolean zap_signal_parse_dvb_file ( const char *file, Zap *zap );

static void zap_treeview_append ( const char *channel, uint16_t apid, uint16_t vpid, Zap *zap )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( zap->treeview );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );
	if ( ind >= UINT16_MAX ) return;

	gtk_list_store_append ( GTK_LIST_STORE ( model ), &iter );
	gtk_list_store_set    ( GTK_LIST_STORE ( model ), &iter,
				COL_NUM, ind + 1,
				COL_CHL, channel,
				COL_VPID, vpid,
				COL_APID, apid,
				-1 );
}

static void zap_signal_trw_act ( GtkTreeView *tree_view, GtkTreePath *path, G_GNUC_UNUSED GtkTreeViewColumn *column, Zap *zap )
{
	uint8_t num_dmx = (uint8_t)gtk_combo_box_get_active ( GTK_COMBO_BOX ( zap->combo_dmx ) );

	uint8_t descr_num = out_demux_n[num_dmx].descr_num; // enum dmx_output

	const char *file = gtk_entry_buffer_get_text ( gtk_entry_get_buffer ( zap->entry_file ) );

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

	const char *file_rec = gtk_entry_buffer_get_text ( gtk_entry_get_buffer ( zap->entry_rec ) );

	g_autofree char *date = time_to_str ();
	g_autofree char *dir = g_path_get_dirname ( file_rec );

	char file_new[PATH_MAX];
	sprintf ( file_new, "%s/%s-%s.ts", dir, date, zap->channel );

	GtkEntryBuffer *bfr = gtk_entry_buffer_new ( file_new, -1 );
	gtk_entry_set_buffer ( zap->entry_rec, bfr );

	g_signal_emit_by_name ( zap, "zap-set-data", descr_num, zap->channel, file );
}

static gboolean zap_treeview_drag_drop ( G_GNUC_UNUSED GtkDropTarget *target, const GValue *value, G_GNUC_UNUSED double x, G_GNUC_UNUSED double y, Zap *zap )
{
	if ( G_VALUE_HOLDS ( value, G_TYPE_FILE ) )
	{
		g_autoptr(GFile) file = g_value_get_object ( value );
		g_autofree char *filename = g_file_get_path ( file );

		zap_signal_parse_dvb_file ( filename, zap );
	}
	else if ( G_VALUE_HOLDS ( value, G_TYPE_STRING ) )
		zap_signal_parse_dvb_file ( g_value_get_string ( value ), zap );

	return TRUE;
}

static void zap_create_treeview_drop ( Zap *zap )
{
	GType types[2] = { G_TYPE_FILE, G_TYPE_STRING };

	GtkDropTarget *dest = gtk_drop_target_new ( G_TYPE_INVALID, GDK_ACTION_COPY );
	gtk_drop_target_set_gtypes ( dest, types, G_N_ELEMENTS (types) );
	g_signal_connect ( dest, "drop", G_CALLBACK ( zap_treeview_drag_drop ), zap );

	gtk_widget_add_controller ( GTK_WIDGET ( zap ), GTK_EVENT_CONTROLLER (dest) );
}

static GtkScrolledWindow * zap_create_treeview_scroll ( Zap *zap )
{
	GtkScrolledWindow *scroll = (GtkScrolledWindow *)gtk_scrolled_window_new ();
	gtk_scrolled_window_set_policy ( scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_visible ( GTK_WIDGET ( scroll ), TRUE );
	gtk_widget_set_hexpand ( GTK_WIDGET ( scroll ), TRUE );
	gtk_widget_set_vexpand ( GTK_WIDGET ( scroll ), TRUE );

	GtkListStore *store = gtk_list_store_new ( NUM_COLS, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING );

	zap->treeview = (GtkTreeView *)gtk_tree_view_new_with_model ( GTK_TREE_MODEL ( store ) );
	gtk_widget_set_visible ( GTK_WIDGET ( zap->treeview ), TRUE );

	zap_create_treeview_drop ( zap );

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	struct Column { const char *name; const char *type; uint8_t num; } column_n[] =
	{
		{ "Num",     "text",   COL_NUM  },
		{ "Channel", "text",   COL_CHL  },
		{ "Video",   "text",   COL_VPID },
		{ "Audio",   "text",   COL_APID },
		{ "File",    "text",   COL_FILE }
	};

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( column_n ); c++ )
	{
		renderer = gtk_cell_renderer_text_new ();

		column = gtk_tree_view_column_new_with_attributes ( column_n[c].name, renderer, column_n[c].type, column_n[c].num, NULL );
		if ( c == COL_VPID || c == COL_APID || c == COL_FILE ) gtk_tree_view_column_set_visible ( column, FALSE );
		gtk_tree_view_append_column ( zap->treeview, column );
	}

	gtk_scrolled_window_set_child ( scroll, GTK_WIDGET ( zap->treeview ) );

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
		if ( entry->channel  ) zap_treeview_append ( entry->channel,  ( entry->audio_pid ) ? entry->audio_pid[0] : 0, ( entry->video_pid ) ? entry->video_pid[0] : 0, zap );
		if ( entry->vchannel ) zap_treeview_append ( entry->vchannel, ( entry->audio_pid ) ? entry->audio_pid[0] : 0, ( entry->video_pid ) ? entry->video_pid[0] : 0, zap );
	}

	dvb_file_free ( dvb_file );

	GtkEntryBuffer *bfr = gtk_entry_buffer_new ( file, -1 );
	gtk_entry_set_buffer ( zap->entry_file, bfr );

	return TRUE;
}

static void zap_file_open_response ( GtkFileChooserDialog *dialog, int response, Zap *zap )
{
	if ( response == GTK_RESPONSE_ACCEPT )
	{
		GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);

		g_autoptr(GFile) file = gtk_file_chooser_get_file ( chooser );

		g_autofree char *filename = g_file_get_path ( file );

		zap_signal_parse_dvb_file ( filename, zap );
	}

	gtk_window_destroy ( GTK_WINDOW ( dialog ) );
}

static void zap_file_open ( const char *path, const char *icon, uint8_t num, Zap *zap )
{
	GtkWindow *window = NULL;

	GtkFileChooserDialog *dialog = ( GtkFileChooserDialog *)gtk_file_chooser_dialog_new (
		" ", window, num, _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Open"), GTK_RESPONSE_ACCEPT, NULL );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), icon );

	g_autoptr(GFile) file_d = g_file_new_for_path ( path );
	gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( dialog ), file_d, NULL );

	gtk_file_chooser_set_select_multiple ( GTK_FILE_CHOOSER ( dialog ), FALSE );

	gtk_widget_show ( GTK_WIDGET ( dialog ) );

 	g_signal_connect ( dialog, "response", G_CALLBACK ( zap_file_open_response ), zap );
}

static void zap_signal_file_open ( G_GNUC_UNUSED GtkEntry *entry, GtkEntryIconPosition icon_pos, Zap *zap )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		zap_file_open ( g_get_home_dir (), "document-open", GTK_FILE_CHOOSER_ACTION_OPEN, zap );
	}
}

static void zap_clicked_clear ( G_GNUC_UNUSED GtkButton *button, Zap *zap )
{
	GtkEntryBuffer *bfr = gtk_entry_buffer_new ( "", -1 );
	gtk_entry_set_buffer ( zap->entry_file, bfr );

	gtk_list_store_clear ( GTK_LIST_STORE ( gtk_tree_view_get_model ( zap->treeview ) ) );
}

static void zap_file_save_response ( GtkFileChooserDialog *dialog, int response, Zap *zap )
{
	if ( response == GTK_RESPONSE_ACCEPT )
	{
		GtkFileChooser *chooser = GTK_FILE_CHOOSER ( dialog );

		g_autoptr(GFile) file = gtk_file_chooser_get_file ( chooser );

		g_autofree char *filename = g_file_get_path ( file );

		if ( filename ) { GtkEntryBuffer *bfr = gtk_entry_buffer_new ( filename, -1 ); gtk_entry_set_buffer ( zap->entry_rec, bfr ); }
	}

	gtk_window_destroy ( GTK_WINDOW ( dialog ) );
}

static void zap_file_save ( const char *path, const char *file, const char *icon, uint8_t num, Zap *zap )
{
	GtkWindow *window = NULL;

	GtkFileChooserDialog *dialog = ( GtkFileChooserDialog *)gtk_file_chooser_dialog_new (
		" ", window, num, _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Save"), GTK_RESPONSE_ACCEPT, NULL );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), icon );

	g_autoptr(GFile) file_d = g_file_new_for_path ( path );
	gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( dialog ), file_d, NULL );

	gtk_file_chooser_set_current_name ( GTK_FILE_CHOOSER ( dialog ), file );

	gtk_widget_show ( GTK_WIDGET ( dialog ) );

 	g_signal_connect ( dialog, "response", G_CALLBACK ( zap_file_save_response ), zap );
}

static void zap_signal_record_file ( G_GNUC_UNUSED GtkEntry *entry, GtkEntryIconPosition icon_pos, Zap *zap )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		const char *file_rec = gtk_entry_buffer_get_text ( gtk_entry_get_buffer ( zap->entry_rec ) );

		g_autofree char *name = g_path_get_basename ( file_rec );

		zap_file_save ( g_get_home_dir (), name, "document-save", GTK_FILE_CHOOSER_ACTION_SAVE, zap );
	}
}

static void zap_set_active_toggled_block ( ulong signal_id, gboolean active, GtkCheckButton *toggle )
{
	g_signal_handler_block   ( toggle, signal_id );

	gtk_check_button_set_active ( toggle, active );

	g_signal_handler_unblock ( toggle, signal_id );
}

static void zap_signal_toggled_record ( GtkCheckButton *button, Zap *zap )
{
	gboolean fe_lock = FALSE;
	g_signal_emit_by_name ( zap, "zap-get-felock", &fe_lock );

	GtkWindow *window = NULL;

	if ( !fe_lock || !zap->channel )
	{
		zap_set_active_toggled_block ( zap->rec_signal_id, FALSE, button );

		dvb5_message_dialog ( "", "Zap?", GTK_MESSAGE_WARNING, window );

		return;
	}

	uint8_t adapter = 0;
	g_signal_emit_by_name ( zap, "zap-get-adapter", &adapter );

	const char *res = NULL;
	const char *file_rec = gtk_entry_buffer_get_text ( gtk_entry_get_buffer ( zap->entry_rec ) );

	gboolean active = gtk_check_button_get_active ( button );

	if ( !active )
	{
		zap->dm->stop_rec = 1; zap->dm->total_rec = 0;
	}
	else 
	{
		zap->dm->stop_rec = 0; zap->dm->total_rec = 0;
		res = dvr_rec_create ( adapter, file_rec, zap->dm );
	}

	if ( res )
	{
		zap_set_active_toggled_block ( zap->rec_signal_id, FALSE, button );

		dvb5_message_dialog ( "", res, GTK_MESSAGE_WARNING, window );
	}
}

static const char * zap_handler_get_size ( Zap *zap )
{
	if ( !zap->dm->total_rec ) return NULL;

	char *str_size = g_format_size ( zap->dm->total_rec );

	return str_size;
}

static GtkBox * zap_set_record_file ( const char *file, Zap *zap )
{
	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkEntryBuffer *bfr = gtk_entry_buffer_new ( file, -1 );

	zap->entry_rec = (GtkEntry *)gtk_entry_new ();
	gtk_widget_set_hexpand ( GTK_WIDGET ( zap->entry_rec ), TRUE );
	gtk_entry_set_buffer ( zap->entry_rec, bfr );
	// g_object_set ( zap->entry_rec, "editable", FALSE, NULL );
	gtk_entry_set_icon_from_icon_name ( zap->entry_rec, GTK_ENTRY_ICON_SECONDARY, "folder" );

	g_signal_connect ( zap->entry_rec, "icon-press", G_CALLBACK ( zap_signal_record_file ), zap );

	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_resource ( "/dvbv5/dvb-rec.png", NULL );
	GtkImage *image = (GtkImage *)gtk_image_new_from_pixbuf ( pixbuf );

	zap->check_rec = (GtkCheckButton *)gtk_check_button_new ();
	zap->rec_signal_id = g_signal_connect ( zap->check_rec, "toggled", G_CALLBACK ( zap_signal_toggled_record ), zap );

	gtk_box_append ( h_box, GTK_WIDGET ( image ) );
	gtk_box_append ( h_box, GTK_WIDGET ( zap->check_rec ) );
	gtk_box_append ( h_box, GTK_WIDGET ( zap->entry_rec ) );

	gtk_widget_set_visible ( GTK_WIDGET ( image ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( zap->entry_rec ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( zap->check_rec ), TRUE );

	gtk_box_append ( v_box, GTK_WIDGET ( h_box ) );

	return v_box;
}

static int zap_pid_play ( Zap *zap )
{
	int pid = 0, SIZE = 1024;
	const char *file = gtk_entry_buffer_get_text ( gtk_entry_get_buffer ( zap->entry_play ) );

	char line[SIZE];
	FILE *fp = popen ( "ps -T -f -o pid:1,args", "r" );

	while ( !feof (fp) )
	{
		if ( fgets ( line, SIZE, fp ) )
		{
			if ( g_strrstr ( line, file ) ) pid = (int)strtoul ( line, NULL, 10 );
		}
	}

	pclose ( fp );

	return pid;
}

static gboolean zap_status_play ( Zap *zap )
{
	if ( zap->exit ) return FALSE;

	gboolean active = gtk_check_button_get_active ( zap->check_play );

	if ( !active ) return FALSE;

	if ( active )
	{
		zap->pid = zap_pid_play ( zap );

		if ( zap->pid )
			return TRUE;
		else
			{ zap_set_active_toggled_block ( zap->play_signal_id, FALSE, zap->check_play ); return FALSE; }
	}

	return TRUE;
}

static void zap_signal_toggled_play ( GtkCheckButton *button, Zap *zap )
{
	gboolean fe_lock = FALSE;
	g_signal_emit_by_name ( zap, "zap-get-felock", &fe_lock );

	GtkWindow *window = NULL;

	if ( !fe_lock || !zap->channel )
	{
		zap_set_active_toggled_block ( zap->play_signal_id, FALSE, button );

		dvb5_message_dialog ( "", "Zap?", GTK_MESSAGE_WARNING, window );

		return;
	}

	const char *file = gtk_entry_buffer_get_text ( gtk_entry_get_buffer ( zap->entry_play ) );

	gboolean active = gtk_check_button_get_active ( button );

	if ( active )
	{
		GError *error = NULL;
		GAppInfo *app = g_app_info_create_from_commandline ( file, NULL, G_APP_INFO_CREATE_NONE, &error );

		g_app_info_launch ( app, NULL, NULL, &error );

		if ( error )
		{
			dvb5_message_dialog ( "", error->message, GTK_MESSAGE_ERROR, window );

			g_error_free ( error );

			zap_set_active_toggled_block ( zap->play_signal_id, FALSE, button );
		}
		else
			g_timeout_add_seconds ( 1, (GSourceFunc)zap_status_play, zap );

		g_object_unref ( app );
	}
	else
	{
		if ( zap->pid ) kill ( zap->pid, SIGINT );
	}
}

static GtkBox * zap_set_play_file ( Zap *zap )
{
	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkEntryBuffer *bfr = gtk_entry_buffer_new ( "mplayer -nocache /dev/dvb/adapter0/dvr0", -1 );

	zap->entry_play = (GtkEntry *)gtk_entry_new ();
	gtk_widget_set_hexpand ( GTK_WIDGET ( zap->entry_play ), TRUE );
	gtk_entry_set_buffer ( zap->entry_play, bfr );
	g_object_set ( zap->entry_play, "editable", TRUE, NULL );

	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_resource ( "/dvbv5/dvb-start.png", NULL );
	GtkImage *image = (GtkImage *)gtk_image_new_from_pixbuf ( pixbuf );

	zap->check_play = (GtkCheckButton *)gtk_check_button_new ();
	zap->play_signal_id = g_signal_connect ( zap->check_play, "toggled", G_CALLBACK ( zap_signal_toggled_play ), zap );

	gtk_box_append ( h_box, GTK_WIDGET ( image ) );
	gtk_box_append ( h_box, GTK_WIDGET ( zap->check_play ) );
	gtk_box_append ( h_box, GTK_WIDGET ( zap->entry_play ) );

	gtk_widget_set_visible ( GTK_WIDGET ( image ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( zap->check_play ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( zap->entry_play ), TRUE );

	gtk_box_append ( v_box, GTK_WIDGET ( h_box ) );

	return v_box;
}

static void zap_handler_stop ( Zap *zap )
{
	zap_set_active_toggled_block ( zap->rec_signal_id,  FALSE, zap->check_rec  );
	zap_set_active_toggled_block ( zap->play_signal_id, FALSE, zap->check_play );

	zap->dm->stop_rec = 1;
	zap->dm->total_rec = 0;

	if ( zap->pid ) kill ( zap->pid, SIGINT );

	if ( zap->channel ) { free ( zap->channel ); zap->channel = NULL; }
}

static void zap_init ( Zap *zap )
{
	zap->dm = g_new0 ( DwrRecMonitor, 1 );
	zap->dm->stop_rec  = 1;
	zap->dm->total_rec = 0;

	zap->exit = FALSE;
	zap->pid = 0;

	GtkBox *box = GTK_BOX ( zap );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( box ), GTK_ORIENTATION_VERTICAL );
	gtk_box_set_spacing ( box, 10 );
	gtk_widget_set_visible ( GTK_WIDGET ( box ), TRUE );

	gtk_widget_set_margin_top    ( GTK_WIDGET ( box ), 10 );
	gtk_widget_set_margin_bottom ( GTK_WIDGET ( box ), 10 );
	gtk_widget_set_margin_start  ( GTK_WIDGET ( box ), 10 );
	gtk_widget_set_margin_end    ( GTK_WIDGET ( box ), 10 );

	gtk_box_append ( box, GTK_WIDGET ( zap_create_treeview_scroll ( zap ) ) );

	g_signal_connect ( zap->treeview, "row-activated", G_CALLBACK ( zap_signal_trw_act ), zap );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkButton *button_clear = (GtkButton *)gtk_button_new_from_icon_name ( "edit-clear" );
	gtk_widget_set_visible ( GTK_WIDGET ( button_clear ), TRUE );

	GtkEntryBuffer *bfr = gtk_entry_buffer_new ( "dvb_channel.conf", -1 );
	zap->entry_file = (GtkEntry *)gtk_entry_new ();
	gtk_widget_set_hexpand ( GTK_WIDGET ( zap->entry_file ), TRUE );
	gtk_entry_set_buffer ( zap->entry_file, bfr );
	// g_object_set ( zap->entry_file, "editable", FALSE, NULL );
	gtk_entry_set_icon_from_icon_name ( zap->entry_file, GTK_ENTRY_ICON_SECONDARY, "folder" );
	g_signal_connect ( zap->entry_file, "icon-press", G_CALLBACK ( zap_signal_file_open ), zap );

	g_signal_connect ( button_clear, "clicked", G_CALLBACK ( zap_clicked_clear ), zap );

	const char *icon = "info";
	gtk_entry_set_icon_from_icon_name ( zap->entry_file, GTK_ENTRY_ICON_PRIMARY, icon );
	gtk_entry_set_icon_tooltip_text ( GTK_ENTRY ( zap->entry_file ), GTK_ENTRY_ICON_PRIMARY, "Format only DVBV5" );
	gtk_box_append ( h_box, GTK_WIDGET ( zap->entry_file ) );

	zap->combo_dmx = (GtkComboBoxText *) gtk_combo_box_text_new ();
	zap_combo_dmx_add ( G_N_ELEMENTS ( out_demux_n ), zap );
	gtk_box_append ( h_box, GTK_WIDGET ( zap->combo_dmx ) );

	gtk_widget_set_visible ( GTK_WIDGET ( zap->entry_file ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( zap->combo_dmx  ), TRUE );

	gtk_box_append ( h_box, GTK_WIDGET ( button_clear ) );

	char file_rec[PATH_MAX];
	sprintf ( file_rec, "%s/%s.ts", g_get_home_dir (), "Record" );

	GtkBox *box_rec = zap_set_record_file ( file_rec, zap );
	gtk_box_append ( box, GTK_WIDGET ( box_rec ) );

	GtkBox *box_play = zap_set_play_file ( zap );
	gtk_box_append ( box, GTK_WIDGET ( box_play ) );

	gtk_box_append ( box, GTK_WIDGET ( h_box ) );

	zap->channel = NULL;

	g_signal_connect ( zap, "zap-stop",     G_CALLBACK ( zap_handler_stop ), NULL );
	g_signal_connect ( zap, "zap-get-size", G_CALLBACK ( zap_handler_get_size ), NULL );
}

static void zap_finalize ( GObject *object )
{
	Zap *zap = ZAP_BOX ( object );

	zap->exit = TRUE;
	if ( zap->pid ) kill ( zap->pid, SIGINT );

	free ( zap->dm );
	if ( zap->channel ) free ( zap->channel );

	G_OBJECT_CLASS (zap_parent_class)->finalize (object);
}

static void zap_class_init ( ZapClass *class )
{
	G_OBJECT_CLASS (class)->finalize = zap_finalize;

	g_signal_new ( "zap-get-size", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_STRING, 0 );

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

