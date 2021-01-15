/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "zap.h"
#include "control.h"

#include <linux/dvb/dmx.h>

const OutDemux out_demux_n[] =
{
	{ DMX_OUT_DECODER, 	"DMX_OUT_DECODER" 	},
	{ DMX_OUT_TAP, 		"DMX_OUT_TAP"		},
	{ DMX_OUT_TS_TAP, 	"DMX_OUT_TS_TAP" 	},
	{ DMX_OUT_TSDEMUX_TAP, 	"DMX_OUT_TSDEMUX_TAP" 	},
	{ DMX_OUT_ALL_PIDS, 	"DMX_OUT_ALL_PIDS" 	},
	{ DMX_OUT_OFF, 		"DMX_OUT_OFF"		}
};

G_DEFINE_TYPE ( Zap, zap, GTK_TYPE_BOX )

u_int8_t zap_get_dmx ( u_int8_t num_dmx )
{
	return out_demux_n[num_dmx].descr_num;
}

static void zap_treeview_stop_dmx_rec_all_toggled ( Zap *zap )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( zap->treeview );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );
	if ( ind == 0 ) return;

	gboolean valid;
	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid;
		  valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
#ifndef LIGHT
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PRW, FALSE, -1 );
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PRW_BITR, 0, -1 );
#endif
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_REC, FALSE, -1 );
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_REC_SIZE, "", -1 );
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_REC_FILE, "", -1 );
	}
}

#ifndef LIGHT
void zap_set_active_toggled_block ( ulong signal_id, gboolean active, GtkCheckButton *toggle )
{
	g_signal_handler_block   ( toggle, signal_id );

	gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( toggle ), active );

	g_signal_handler_unblock ( toggle, signal_id );
}
#endif

void zap_stop_toggled_all ( Zap *zap )
{
#ifndef LIGHT
	zap_set_active_toggled_block ( zap->prw_signal_id, FALSE, zap->toggled_prw );
	zap_set_active_toggled_block ( zap->rec_signal_id, FALSE, zap->toggled_rec );
	zap_set_active_toggled_block ( zap->stm_signal_id, FALSE, zap->toggled_stm );

	gtk_widget_set_sensitive ( GTK_WIDGET ( zap->toggled_prw ), TRUE );
	gtk_widget_set_sensitive ( GTK_WIDGET ( zap->toggled_rec ), TRUE );
	gtk_widget_set_sensitive ( GTK_WIDGET ( zap->toggled_stm ), TRUE );

	zap->rec_status = FALSE;
	zap->stm_status = FALSE;
	zap->prw_status = FALSE;
#endif

	zap_treeview_stop_dmx_rec_all_toggled ( zap );
}

void zap_treeview_append ( const char *channel, u_int16_t sid, u_int16_t apid, u_int16_t vpid, u_int32_t freq, Zap *zap )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( zap->treeview );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );
	if ( ind >= UINT_MAX ) return;

	gtk_list_store_append ( GTK_LIST_STORE ( model ), &iter );
	gtk_list_store_set    ( GTK_LIST_STORE ( model ), &iter,
				COL_NUM, ind + 1,
				COL_CHAHHEL, channel,
				COL_FREQ, freq,
				COL_SID, sid,
				COL_VPID, vpid,
				COL_APID, apid,
				-1 );
}

#ifndef LIGHT
static void zap_prw_toggled ( G_GNUC_UNUSED GtkCellRendererToggle *toggle, char *path_str, Zap *zap )
{
	g_signal_emit_by_name ( zap, "button-toggled-prw", path_str );
}
#endif

static void zap_rec_toggled ( G_GNUC_UNUSED GtkCellRendererToggle *toggle, char *path_str, Zap *zap )
{
	g_signal_emit_by_name ( zap, "button-toggled-rec", path_str );
}

static GtkScrolledWindow * zap_create_treeview_scroll ( Zap *zap )
{
	GtkScrolledWindow *scroll = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_visible ( GTK_WIDGET ( scroll ), TRUE );

	GtkListStore *store = gtk_list_store_new ( NUM_COLS, G_TYPE_UINT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, 
		G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING );

	zap->treeview = (GtkTreeView *)gtk_tree_view_new_with_model ( GTK_TREE_MODEL ( store ) );
	gtk_drag_dest_set ( GTK_WIDGET ( zap->treeview ), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY );
	gtk_drag_dest_add_uri_targets  ( GTK_WIDGET ( zap->treeview ) );
	gtk_widget_set_visible ( GTK_WIDGET ( zap->treeview ), TRUE );

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	struct Column { const char *name; const char *type; u_int8_t num; } column_n[] =
	{
		{ "Num",        "text",   COL_NUM },
		{ "Prw",        "active", COL_PRW },
		{ "Rec",        "active", COL_REC },
		{ "Channel",    "text",   COL_CHAHHEL  },
		{ "Kbps",       "text",   COL_PRW_BITR },
		{ "Size",       "text",   COL_REC_SIZE },
		{ "Mhz", 	"text",   COL_FREQ },
		{ "SID", 	"text",   COL_SID  },
		{ "Video",      "text",   COL_VPID },
		{ "Audio",      "text",   COL_APID },
		{ "File rec",   "text",   COL_REC_FILE }
	};

	u_int8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( column_n ); c++ )
	{
#ifdef LIGHT
		if ( c == COL_PRW || c == COL_PRW_BITR ) continue;
#endif
		if ( c == COL_PRW || c == COL_REC )
		{
			renderer = gtk_cell_renderer_toggle_new ();
#ifndef LIGHT
			if ( c == COL_PRW ) g_signal_connect ( renderer, "toggled", G_CALLBACK ( zap_prw_toggled ), zap );
#endif
			if ( c == COL_REC ) g_signal_connect ( renderer, "toggled", G_CALLBACK ( zap_rec_toggled ), zap );
		}
		else
			renderer = gtk_cell_renderer_text_new ();

		column = gtk_tree_view_column_new_with_attributes ( column_n[c].name, renderer, column_n[c].type, column_n[c].num, NULL );
		if ( c == COL_REC_FILE ) gtk_tree_view_column_set_visible ( column, FALSE );
		gtk_tree_view_append_column ( zap->treeview, column );
	}

	gtk_container_add ( GTK_CONTAINER ( scroll ), GTK_WIDGET ( zap->treeview ) );
	g_object_unref ( G_OBJECT (store) );

	return scroll;
}

static void zap_combo_dmx_add ( u_int8_t n_elm, Zap *zap )
{
	u_int8_t c = 0; for ( c = 0; c < n_elm; c++ )
		gtk_combo_box_text_append_text ( zap->combo_dmx, out_demux_n[c].name );

	gtk_combo_box_set_active ( GTK_COMBO_BOX ( zap->combo_dmx ), 2 );
}

static void zap_init ( Zap *zap )
{
	zap->monitor = g_new0 ( Monitor, 1 );

	GtkBox *box = GTK_BOX ( zap );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( box ), GTK_ORIENTATION_VERTICAL );
	gtk_widget_set_visible ( GTK_WIDGET ( box ), TRUE );

	zap->rec_status = FALSE;
	zap->stm_status = FALSE;
	zap->prw_status = FALSE;

	zap->host = g_strdup ( "127.0.0.1" );
	zap->port = 64000;

	gtk_box_pack_start ( box, GTK_WIDGET ( zap_create_treeview_scroll ( zap ) ), TRUE, TRUE, 0 );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

#ifndef LIGHT
	GtkLabel *label = (GtkLabel *)gtk_label_new ( "Dvr:  " );
	gtk_widget_set_visible ( GTK_WIDGET ( label ), TRUE );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( label ), FALSE, FALSE, 0 );

	zap->toggled_prw = (GtkCheckButton *)gtk_check_button_new_with_label ( "Preview" );
	zap->toggled_rec = (GtkCheckButton *)gtk_check_button_new_with_label ( "Record" );
	zap->toggled_stm = (GtkCheckButton *)gtk_check_button_new_with_label ( "Stream" );

	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->toggled_prw ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->toggled_rec ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->toggled_stm ), FALSE, FALSE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( zap->toggled_prw ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( zap->toggled_rec ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( zap->toggled_stm ), TRUE );
#endif

	GtkButton *button_clear = (GtkButton *)gtk_button_new_from_icon_name ( "edit-clear", GTK_ICON_SIZE_MENU );
	g_signal_connect_swapped ( button_clear, "clicked", G_CALLBACK ( gtk_list_store_clear ), GTK_LIST_STORE ( gtk_tree_view_get_model ( zap->treeview ) ) );
	gtk_widget_set_visible ( GTK_WIDGET ( button_clear ), TRUE );

	gtk_box_pack_end   ( h_box, GTK_WIDGET ( button_clear ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( box, GTK_WIDGET ( h_box ), FALSE, FALSE, 5 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	zap->entry_file = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( zap->entry_file, "dvb_channel.conf" );
	g_object_set ( zap->entry_file, "editable", FALSE, NULL );
	gtk_entry_set_icon_from_icon_name ( zap->entry_file, GTK_ENTRY_ICON_SECONDARY, "folder" );

	const char *icon = ( control_check_icon_theme ( "dvb-info" ) ) ? "dvb-info" : "info";
	gtk_entry_set_icon_from_icon_name ( zap->entry_file, GTK_ENTRY_ICON_PRIMARY, icon );
	gtk_entry_set_icon_tooltip_text ( GTK_ENTRY ( zap->entry_file ), GTK_ENTRY_ICON_PRIMARY, "Format only DVBV5" );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->entry_file ), TRUE, TRUE, 0 );

	zap->combo_dmx = (GtkComboBoxText *) gtk_combo_box_text_new ();
	zap_combo_dmx_add ( G_N_ELEMENTS ( out_demux_n ), zap );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->combo_dmx ), TRUE, TRUE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( zap->entry_file ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( zap->combo_dmx  ), TRUE );

	gtk_box_pack_start ( box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );
}

static void zap_finalize ( GObject *object )
{
	G_OBJECT_CLASS (zap_parent_class)->finalize (object);
}

static void zap_class_init ( ZapClass *class )
{
	G_OBJECT_CLASS (class)->finalize = zap_finalize;

#ifndef LIGHT
	g_signal_new ( "button-toggled-prw", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );
#endif
	g_signal_new ( "button-toggled-rec", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );
}

Zap * zap_new (void)
{
	return g_object_new ( ZAP_TYPE_BOX, NULL );
}

