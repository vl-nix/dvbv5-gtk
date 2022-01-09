/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/gpl-2.0.html
*/

#include "scan.h"
#include "lnb.h"
#include "file.h"

#include <glib/gi18n.h>

enum io_file
{
	INT_F,
	OUT_F
};

struct _Scan
{
	GtkGrid parent_instance;

	GtkSpinButton *spinbutton[6];
	GtkCheckButton *checkbutton[4];
	GtkComboBoxText *combo_lnb;
	GtkComboBoxText *combo_lna;
	GtkButton *button_lnb;
	GtkEntry *entry_int, *entry_out;
	GtkComboBoxText *combo_int, *combo_out;
};

G_DEFINE_TYPE ( Scan, scan, GTK_TYPE_GRID )

static void scan_handler_get_data ( Scan *scan )
{
	uint8_t adapter   = (uint8_t)gtk_spin_button_get_value_as_int ( scan->spinbutton[0] );
	uint8_t frontend  = (uint8_t)gtk_spin_button_get_value_as_int ( scan->spinbutton[1] );
	uint8_t demux     = (uint8_t)gtk_spin_button_get_value_as_int ( scan->spinbutton[2] );
	uint8_t time_mult = (uint8_t)gtk_spin_button_get_value_as_int ( scan->spinbutton[3] );

	gboolean new_freqs  = gtk_check_button_get_active ( scan->checkbutton[0] );
	gboolean get_detect = gtk_check_button_get_active ( scan->checkbutton[1] );
	gboolean get_nit    = gtk_check_button_get_active ( scan->checkbutton[2] );
	gboolean other_nit  = gtk_check_button_get_active ( scan->checkbutton[3] );

	int8_t sat_n = (int8_t)gtk_spin_button_get_value_as_int ( scan->spinbutton[4] );
	uint8_t diseqc_w = (uint8_t)gtk_spin_button_get_value_as_int ( scan->spinbutton[5] );

	const char *lnb = gtk_combo_box_get_active_id ( GTK_COMBO_BOX ( scan->combo_lnb ) );
	g_autofree char *lna = gtk_combo_box_text_get_active_text (scan->combo_lna );

	const char *file_i = gtk_entry_buffer_get_text ( gtk_entry_get_buffer ( scan->entry_int ) );
	const char *file_o = gtk_entry_buffer_get_text ( gtk_entry_get_buffer ( scan->entry_out ) );

	g_autofree char *fmi = gtk_combo_box_text_get_active_text (scan->combo_int );
	g_autofree char *fmo = gtk_combo_box_text_get_active_text (scan->combo_out );

	g_signal_emit_by_name ( scan, "scan-set-data", adapter, frontend, demux, time_mult, new_freqs, get_detect, get_nit, other_nit, 
		sat_n, diseqc_w, lnb, lna, file_i, file_o, fmi, fmo );
}

static void scan_signal_changed ( G_GNUC_UNUSED GtkSpinButton *spinbutton, Scan *scan )
{
	uint8_t val_a = (uint8_t)gtk_spin_button_get_value_as_int ( scan->spinbutton[0] );
	uint8_t val_f = (uint8_t)gtk_spin_button_get_value_as_int ( scan->spinbutton[1] );
	uint8_t val_d = (uint8_t)gtk_spin_button_get_value_as_int ( scan->spinbutton[2] );

	g_signal_emit_by_name ( scan, "scan-set-af", "Adapter", val_a, "Frontend", val_f, "Demux", val_d );
}

static GtkLabel * scan_create_label ( const char *text )
{
	GtkLabel *label = (GtkLabel *)gtk_label_new ( text );
	gtk_widget_set_halign ( GTK_WIDGET ( label ), GTK_ALIGN_START );
	gtk_widget_set_visible ( GTK_WIDGET ( label ), TRUE );

	return label;
}

static GtkSpinButton * scan_create_spinbutton ( int16_t min, int16_t max, uint8_t step, int16_t value, uint8_t num, const char *name, Scan *scan )
{
	GtkSpinButton *spinbutton = (GtkSpinButton *)gtk_spin_button_new_with_range ( min, max, step );
	gtk_widget_set_name ( GTK_WIDGET ( spinbutton ), name );
	gtk_spin_button_set_value ( spinbutton, value );

	scan->spinbutton[num] = spinbutton;

	if ( num == 0 || num == 1 || num == 2 ) g_signal_connect ( spinbutton, "value-changed", G_CALLBACK ( scan_signal_changed ), scan );

	gtk_widget_set_visible ( GTK_WIDGET ( spinbutton ), TRUE );

	return spinbutton;
}

static GtkCheckButton * scan_create_checkbutton ( int16_t value, uint8_t num, const char *name, Scan *scan )
{
	GtkCheckButton *checkbutton = (GtkCheckButton *)gtk_check_button_new ();
	gtk_widget_set_name ( GTK_WIDGET ( checkbutton ), name );
	gtk_check_button_set_active ( checkbutton, ( value ) ? TRUE : FALSE );

	scan->checkbutton[num] = checkbutton;

	gtk_widget_set_visible ( GTK_WIDGET ( checkbutton ), TRUE );

	return checkbutton;
}

static void scan_append_text_combo_box ( GtkComboBoxText *combo_box, const char *text[], uint8_t indx, int active )
{
	uint8_t c = 0; for ( c = 0; c < indx; c++ )
		gtk_combo_box_text_append_text ( combo_box, text[c] );

	gtk_combo_box_set_active ( GTK_COMBO_BOX ( combo_box ), active );
}

static void scan_signal_clicked_button_lnb ( G_GNUC_UNUSED GtkButton *button, Scan *scan )
{
	const char *lnb_name = gtk_combo_box_get_active_id ( GTK_COMBO_BOX ( scan->combo_lnb ) );

	if ( g_str_has_prefix ( lnb_name, "NONE" ) ) return;

	const char *desc = lnb_get_desc ( lnb_name );

	GtkWindow *window = NULL;
	dvb5_message_dialog ( lnb_name, desc, GTK_MESSAGE_INFO, window );
}

static GtkBox * scan_create_combo_box_lnb ( Scan *scan )
{
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	scan->combo_lnb = (GtkComboBoxText *) gtk_combo_box_text_new ();
	gtk_combo_box_text_append ( scan->combo_lnb, "NONE", "None" );

	uint8_t c = 0; for ( c = 0; c < LNB_ALL; c++ )
	{
		const char *abr  = lnb_get_abr  ( c );
		const char *name = lnb_get_name ( c );

		gtk_combo_box_text_append ( scan->combo_lnb, name, abr );
	}

	gtk_combo_box_set_active ( GTK_COMBO_BOX ( scan->combo_lnb ), 0 );

	scan->button_lnb = (GtkButton *)gtk_button_new_from_icon_name ( "info" );
	g_signal_connect ( scan->button_lnb, "clicked", G_CALLBACK ( scan_signal_clicked_button_lnb ), scan );

	gtk_box_append ( h_box, GTK_WIDGET ( scan->combo_lnb ) );
	gtk_box_append ( h_box, GTK_WIDGET ( scan->button_lnb ) );

	gtk_widget_set_visible ( GTK_WIDGET ( scan->combo_lnb  ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( scan->button_lnb ), TRUE );

	return h_box;
}

static GtkComboBoxText * scan_create_combo_box_lna ( Scan *scan )
{
	const char *lna[] = { "On", "Off", "Auto" };

	scan->combo_lna = (GtkComboBoxText *) gtk_combo_box_text_new ();
	scan_append_text_combo_box ( scan->combo_lna, lna, G_N_ELEMENTS ( lna ), 2 );

	gtk_widget_set_visible ( GTK_WIDGET ( scan->combo_lna ), TRUE );

	return scan->combo_lna;
}

static GtkComboBoxText * scan_create_combo_box_format ( uint8_t num, Scan *scan )
{
	const char *inp[] = { "DVBV5", "CHANNEL" };
	const char *out[] = { "DVBV5", "VDR", "CHANNEL", "scan" };

	GtkComboBoxText *combo_box = (GtkComboBoxText *) gtk_combo_box_text_new ();

	if ( num == INT_F ) { scan_append_text_combo_box ( combo_box, inp, G_N_ELEMENTS ( inp ), 0 ); scan->combo_int = combo_box; }
	if ( num == OUT_F ) { scan_append_text_combo_box ( combo_box, out, G_N_ELEMENTS ( out ), 0 ); scan->combo_out = combo_box; }

	gtk_widget_set_visible ( GTK_WIDGET ( combo_box ), TRUE );

	return combo_box;
}

static GtkBox * scan_set_initial_output_file ( const char *file, uint8_t num, Scan *scan )
{
	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );

	GtkEntryBuffer *bfr = gtk_entry_buffer_new ( file, -1 );

	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_buffer ( entry, bfr );
	// g_object_set ( entry, "editable", FALSE, NULL );
	gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_SECONDARY, "folder" );

	if ( num == INT_F ) scan->entry_int = entry;
	if ( num == OUT_F ) scan->entry_out = entry;

	gtk_box_append ( v_box, GTK_WIDGET ( entry ) );

	gtk_widget_set_visible ( GTK_WIDGET ( entry ), TRUE );

	return v_box;
}

static void scan_create ( Scan *scan )
{
	GtkGrid *grid = GTK_GRID ( scan );
	gtk_grid_set_row_homogeneous    ( GTK_GRID ( grid ), FALSE );
	gtk_grid_set_column_homogeneous ( GTK_GRID ( grid ), TRUE );
	gtk_grid_set_row_spacing ( grid, 5 );
	gtk_grid_set_column_spacing ( grid, 10 );

	gtk_widget_set_margin_top    ( GTK_WIDGET ( grid ), 10 );
	gtk_widget_set_margin_bottom ( GTK_WIDGET ( grid ), 10 );
	gtk_widget_set_margin_start  ( GTK_WIDGET ( grid ), 10 );
	gtk_widget_set_margin_end    ( GTK_WIDGET ( grid ), 10 );

	gtk_widget_set_visible ( GTK_WIDGET ( grid ), TRUE );

	struct DataDevice { const char *text; int16_t value; const char *text_2; int16_t value_2; } data_n[] =
	{
		{ "Adapter",     0, "Frontend",     0 	},
		{ "Demux",   	 0, "Timeout",      2 	},
		{ "Freqs-only",  0, "Get frontend", 0 	},
		{ "Nit",   	 0, "Other nit",    0 	},
		{ "LNB", 	-1, "DISEqC",      -1   },
		{ "LNA",   	-1, "Wait DISEqC",  0	}
	};

	uint8_t d = 0, spin_num = 0, toggle_num = 0;
	for ( d = 0; d < G_N_ELEMENTS ( data_n ); d++ )
	{
		gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_label ( data_n[d].text ) ), 0, d, 1, 1 );

		if ( d == 0 || d == 1 )
		{
			gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_spinbutton ( 0, 16, 1, data_n[d].value, spin_num++, data_n[d].text, scan ) ), 1, d, 1, 1 );
		}
		else if ( d == 2 || d == 3 )
		{
			gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_checkbutton ( data_n[d].value, toggle_num++, data_n[d].text, scan ) ), 1, d, 1, 1 );
		}
		else
		{
			if ( d == 4 )
				gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_combo_box_lnb ( scan ) ), 1, d, 1, 1 );
			else
				gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_combo_box_lna ( scan ) ), 1, d, 1, 1 );
		}

		gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_label ( data_n[d].text_2 ) ), 2, d, 1, 1 );

		if ( d == 0 || d == 1 )
		{
			if ( d == 0 )
				gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_spinbutton ( 0, 16, 1, data_n[d].value_2, spin_num++, data_n[d].text_2, scan ) ), 3, d, 1, 1 );
			else
				gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_spinbutton ( 1, 250, 1, data_n[d].value_2, spin_num++, data_n[d].text_2, scan ) ), 3, d, 1, 1 );
		}
		else if ( d == 2 || d == 3 )
		{
			gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_checkbutton ( data_n[d].value_2, toggle_num++, data_n[d].text_2, scan ) ), 3, d, 1, 1 );
		}
		else
		{
			if ( d == 4 )
				gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_spinbutton ( -1, 48, 1, data_n[d].value_2, spin_num++, data_n[d].text_2, scan ) ), 3, d, 1, 1 );
			else
				gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_spinbutton ( 0, 250, 1, data_n[d].value_2, spin_num++, data_n[d].text_2, scan ) ), 3, d, 1, 1 );
		}
	}

	g_autofree char *output_file  = g_strconcat ( g_get_home_dir (), "/dvb_channel.conf", NULL );

	gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_set_initial_output_file ( "Initial file", INT_F, scan ) ), 0, d,   2, 1 );
	gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_set_initial_output_file ( output_file,    OUT_F, scan ) ), 2, d++, 2, 1 );

	gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_combo_box_format ( INT_F, scan ) ), 0, d,   2, 1 );
	gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_combo_box_format ( OUT_F, scan ) ), 2, d++, 2, 1 );
}

static void scan_file_open_response ( GtkFileChooserDialog *dialog, int response, Scan *scan )
{
	if ( response == GTK_RESPONSE_ACCEPT )
	{
		GtkFileChooser *chooser = GTK_FILE_CHOOSER ( dialog );

		g_autoptr(GFile) file = gtk_file_chooser_get_file ( chooser );

		g_autofree char *filename = g_file_get_path ( file );

		if ( filename ) { GtkEntryBuffer *bfr = gtk_entry_buffer_new ( filename, -1 ); gtk_entry_set_buffer ( scan->entry_int, bfr ); }
	}

	gtk_window_destroy ( GTK_WINDOW ( dialog ) );
}

static void scan_file_open ( const char *path, const char *icon, uint8_t num, Scan *scan )
{
	GtkWindow *window = NULL;

	GtkFileChooserDialog *dialog = ( GtkFileChooserDialog *)gtk_file_chooser_dialog_new (
		" ", window, num, _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Open"), GTK_RESPONSE_ACCEPT, NULL );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), icon );

	g_autoptr(GFile) file_d = g_file_new_for_path ( path );
	gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( dialog ), file_d, NULL );

	gtk_file_chooser_set_select_multiple ( GTK_FILE_CHOOSER ( dialog ), FALSE );

	gtk_widget_show ( GTK_WIDGET ( dialog ) );

 	g_signal_connect ( dialog, "response", G_CALLBACK ( scan_file_open_response ), scan );
}

static void scan_signal_file_open ( G_GNUC_UNUSED GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED Scan *scan )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		const char *path = ( g_file_test ( "/usr/share/dvb/", G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR ) ) ? "/usr/share/dvb/" : g_get_home_dir ();

		scan_file_open ( path, "document-open", GTK_FILE_CHOOSER_ACTION_OPEN, scan );
	}
}

static void scan_file_save_response ( GtkFileChooserDialog *dialog, int response, Scan *scan )
{
	if ( response == GTK_RESPONSE_ACCEPT )
	{
		GtkFileChooser *chooser = GTK_FILE_CHOOSER ( dialog );

		g_autoptr(GFile) file = gtk_file_chooser_get_file ( chooser );

		g_autofree char *filename = g_file_get_path ( file );

		if ( filename ) { GtkEntryBuffer *bfr = gtk_entry_buffer_new ( filename, -1 ); gtk_entry_set_buffer ( scan->entry_out, bfr ); }
	}

	gtk_window_destroy ( GTK_WINDOW ( dialog ) );
}

static void scan_file_save ( const char *path, const char *file, const char *icon, uint8_t num, Scan *scan )
{
	GtkWindow *window = NULL;

	GtkFileChooserDialog *dialog = ( GtkFileChooserDialog *)gtk_file_chooser_dialog_new (
		" ", window, num, _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Save"), GTK_RESPONSE_ACCEPT, NULL );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), icon );

	g_autoptr(GFile) file_d = g_file_new_for_path ( path );
	gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( dialog ), file_d, NULL );

	if ( file )
		gtk_file_chooser_set_current_name ( GTK_FILE_CHOOSER ( dialog ), file );
	else
		gtk_file_chooser_set_select_multiple ( GTK_FILE_CHOOSER ( dialog ), FALSE );

	gtk_widget_show ( GTK_WIDGET ( dialog ) );

 	g_signal_connect ( dialog, "response", G_CALLBACK ( scan_file_save_response ), scan );
}

static void scan_signal_file_save ( G_GNUC_UNUSED GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED Scan *scan )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		scan_file_save ( g_get_home_dir (), "dvb_channel.conf", "document-save", GTK_FILE_CHOOSER_ACTION_SAVE, scan );
	}
}

static void scan_init ( Scan *scan )
{
	scan_create ( scan );

	g_signal_connect ( scan->entry_int, "icon-press", G_CALLBACK ( scan_signal_file_open ), scan );
	g_signal_connect ( scan->entry_out, "icon-press", G_CALLBACK ( scan_signal_file_save ), scan );

	g_signal_connect ( scan, "scan-get-data", G_CALLBACK ( scan_handler_get_data ), NULL );
}

static void scan_finalize ( GObject *object )
{
	G_OBJECT_CLASS (scan_parent_class)->finalize (object);
}

static void scan_class_init ( ScanClass *class )
{
	G_OBJECT_CLASS (class)->finalize = scan_finalize;

	g_signal_new ( "scan-set-af", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 6, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_UINT );

	g_signal_new ( "scan-get-data", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 0 );

	g_signal_new ( "scan-set-data", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 16, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, 
		G_TYPE_INT, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING );
}

Scan * scan_new ( void )
{
	return g_object_new ( SCAN_TYPE_GRID, NULL );
}

