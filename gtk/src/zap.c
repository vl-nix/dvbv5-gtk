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

G_DEFINE_TYPE ( Zap, zap, GTK_TYPE_BOX )

void zap_set_active_toggled_block ( ulong signal_id, gboolean active, GtkCheckButton *toggle )
{
#ifndef LIGHT
	g_signal_handler_block   ( toggle, signal_id );

	gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( toggle ), active );

	g_signal_handler_unblock ( toggle, signal_id );
#endif
}

void zap_stop_toggled_all ( Zap *zap )
{
	zap_set_active_toggled_block ( zap->prw_signal_id, FALSE, zap->toggled_prw );
	zap_set_active_toggled_block ( zap->rec_signal_id, FALSE, zap->toggled_rec );
	zap_set_active_toggled_block ( zap->stm_signal_id, FALSE, zap->toggled_stm );

	gtk_widget_set_sensitive ( GTK_WIDGET ( zap->toggled_prw ), TRUE );
	gtk_widget_set_sensitive ( GTK_WIDGET ( zap->toggled_rec ), TRUE );
	gtk_widget_set_sensitive ( GTK_WIDGET ( zap->toggled_stm ), TRUE );
	gtk_widget_set_sensitive ( GTK_WIDGET ( zap->toggled_tmo ), TRUE );

	zap->rec_status = FALSE;
	zap->stm_status = FALSE;
	zap->prw_status = FALSE;
}

void zap_treeview_append ( const char *channel, uint16_t sid, uint16_t apid, uint16_t vpid, Zap *zap )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( zap->treeview );

	uint16_t ind = gtk_tree_model_iter_n_children ( model, NULL );
	if ( ind >= UINT16_MAX ) return;

	gtk_list_store_append ( GTK_LIST_STORE ( model ), &iter );
	gtk_list_store_set    ( GTK_LIST_STORE ( model ), &iter,
				COL_NUM, ind + 1,
				COL_CHAHHEL, channel,
				COL_SID, sid,
				COL_VIDEO_PID, vpid,
				COL_AUDIO_PID, apid,
				-1 );
}

static GtkScrolledWindow * zap_create_treeview_scroll ( Zap *zap )
{
	GtkScrolledWindow *scroll = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

	GtkListStore *store = gtk_list_store_new ( NUM_COLS, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT );

	zap->treeview = (GtkTreeView *)gtk_tree_view_new_with_model ( GTK_TREE_MODEL ( store ) );
	gtk_drag_dest_set ( GTK_WIDGET ( zap->treeview ), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY );
	gtk_drag_dest_add_uri_targets  ( GTK_WIDGET ( zap->treeview ) );

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	struct Column { const char *name; const char *type; uint8_t num; } column_n[] =
	{
		{ "Num",        "text",   COL_NUM },
		{ "Channel",    "text",   COL_CHAHHEL },
		{ "SID", 	"text",   COL_SID },
		{ "Video",      "text",   COL_VIDEO_PID },
		{ "Audio",      "text",   COL_AUDIO_PID },
	};

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( column_n ); c++ )
	{
		renderer = gtk_cell_renderer_text_new ();

		column = gtk_tree_view_column_new_with_attributes ( column_n[c].name, renderer, column_n[c].type, column_n[c].num, NULL );
		gtk_tree_view_append_column ( zap->treeview, column );
	}

	gtk_container_add ( GTK_CONTAINER ( scroll ), GTK_WIDGET ( zap->treeview ) );
	g_object_unref ( G_OBJECT (store) );

	return scroll;
}

static void zap_init ( Zap *zap )
{
	GtkBox *box = GTK_BOX ( zap );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( box ), GTK_ORIENTATION_VERTICAL );

	zap->zap_status = FALSE;
	zap->rec_status = FALSE;
	zap->stm_status = FALSE;
	zap->prw_status = FALSE;

	gtk_box_pack_start ( box, GTK_WIDGET ( zap_create_treeview_scroll ( zap ) ), TRUE, TRUE, 0 );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	zap->toggled_prw = (GtkCheckButton *)gtk_check_button_new_with_label ( "Preview" );
	zap->toggled_rec = (GtkCheckButton *)gtk_check_button_new_with_label ( "Record" );
	zap->toggled_stm = (GtkCheckButton *)gtk_check_button_new_with_label ( "Stream" );
	zap->toggled_tmo = (GtkCheckButton *)gtk_check_button_new_with_label ( "Time" );
	gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( zap->toggled_tmo ), TRUE ); // On ( def )
	gtk_widget_set_tooltip_text ( GTK_WIDGET ( zap->toggled_tmo ), "Time stamp ( Preview )" );

	GtkButton *button_clear = (GtkButton *)gtk_button_new_from_icon_name ( "edit-clear", GTK_ICON_SIZE_MENU );
	g_signal_connect_swapped ( button_clear, "clicked", G_CALLBACK ( gtk_list_store_clear ), GTK_LIST_STORE ( gtk_tree_view_get_model ( zap->treeview ) ) );

#ifndef LIGHT
	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->toggled_prw ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->toggled_rec ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->toggled_stm ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->toggled_tmo ), FALSE, FALSE, 0 );
#endif

	gtk_box_pack_end   ( h_box, GTK_WIDGET ( button_clear     ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( box, GTK_WIDGET ( h_box ), FALSE, FALSE, 5 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	zap->entry_file = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( zap->entry_file, "dvb_channel.conf" );
	g_object_set ( zap->entry_file, "editable", FALSE, NULL );
	gtk_entry_set_icon_from_icon_name ( zap->entry_file, GTK_ENTRY_ICON_SECONDARY, "folder" );

#ifdef LIGHT
	gtk_entry_set_icon_from_icon_name ( zap->entry_file, GTK_ENTRY_ICON_PRIMARY, "info" );
#else
	gtk_entry_set_icon_from_icon_name ( zap->entry_file, GTK_ENTRY_ICON_PRIMARY, "dvb-info" );
#endif

	gtk_entry_set_icon_tooltip_text ( GTK_ENTRY ( zap->entry_file ), GTK_ENTRY_ICON_PRIMARY, "Format only DVBV5" );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->entry_file ), TRUE, TRUE, 0 );

	zap->combo_dmx = (GtkComboBoxText *) gtk_combo_box_text_new ();
	gtk_box_pack_start ( h_box, GTK_WIDGET ( zap->combo_dmx ), TRUE, TRUE, 0 );

	gtk_box_pack_start ( box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );
}

static void zap_finalize ( GObject *object )
{
	G_OBJECT_CLASS (zap_parent_class)->finalize (object);
}

static void zap_class_init ( ZapClass *class )
{
	G_OBJECT_CLASS (class)->finalize = zap_finalize;
}

Zap * zap_new (void)
{
	return g_object_new ( ZAP_TYPE_BOX, NULL );
}

