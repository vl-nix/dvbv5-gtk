/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "scan.h"
#include "lnb.h"
#include "control.h"

enum io_file
{
	INT_F,
	OUT_F
};

G_DEFINE_TYPE ( Scan, scan, GTK_TYPE_GRID )

static GtkLabel * scan_create_label ( const char *text )
{
	GtkLabel *label = (GtkLabel *)gtk_label_new ( text );
	gtk_widget_set_halign ( GTK_WIDGET ( label ), GTK_ALIGN_START );

	return label;
}

static GtkSpinButton * scan_create_spinbutton ( int16_t min, int16_t max, uint8_t step, int16_t value, uint8_t num, const char *name, Scan *scan )
{
	GtkSpinButton *spinbutton = (GtkSpinButton *)gtk_spin_button_new_with_range ( min, max, step );
	gtk_widget_set_name ( GTK_WIDGET ( spinbutton ), name );
	gtk_spin_button_set_value ( spinbutton, value );

	scan->spinbutton[num] = spinbutton;

	return spinbutton;
}

static GtkCheckButton * scan_create_checkbutton ( int16_t value, uint8_t num, const char *name, Scan *scan )
{
	GtkCheckButton *checkbutton = (GtkCheckButton *)gtk_check_button_new ();
	gtk_widget_set_name ( GTK_WIDGET ( checkbutton ), name );
	gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( checkbutton ), ( value ) ? TRUE : FALSE );

	scan->checkbutton[num] = checkbutton;

	return checkbutton;
}

static void scan_append_text_combo_box ( GtkComboBoxText *combo_box, const char *text[], uint8_t indx, int active )
{
	uint8_t c = 0; for ( c = 0; c < indx; c++ )
		gtk_combo_box_text_append_text ( combo_box, text[c] );

	gtk_combo_box_set_active ( GTK_COMBO_BOX ( combo_box ), active );
}

static GtkBox * scan_create_combo_box_lnb ( Scan *scan )
{
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

	scan->combo_lnb = (GtkComboBoxText *) gtk_combo_box_text_new ();
	gtk_combo_box_text_append ( scan->combo_lnb, "NONE", "None" );
	lnb_set_name_combo ( scan->combo_lnb );
	gtk_combo_box_set_active ( GTK_COMBO_BOX ( scan->combo_lnb ), 0 );

	scan->button_lnb = control_create_button ( NULL, "dvb-info", "🛈", 16 );

	gtk_box_pack_start ( h_box, GTK_WIDGET ( scan->combo_lnb  ), TRUE, TRUE, 0 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( scan->button_lnb ), TRUE, TRUE, 0 );

	return h_box;
}

static GtkComboBoxText * scan_create_combo_box_lna ( Scan *scan )
{
	const char *lna[] = { "On", "Off", "Auto" };

	scan->combo_lna = (GtkComboBoxText *) gtk_combo_box_text_new ();
	scan_append_text_combo_box ( scan->combo_lna, lna, G_N_ELEMENTS ( lna ), 2 );

	return scan->combo_lna;
}

static GtkComboBoxText * scan_create_combo_box_format ( uint8_t num, Scan *scan )
{
	const char *inp[] = { "DVBV5", "CHANNEL" };
	const char *out[] = { "DVBV5", "VDR", "CHANNEL", "ZAP" };

	GtkComboBoxText *combo_box = (GtkComboBoxText *) gtk_combo_box_text_new ();

	if ( num == INT_F ) { scan_append_text_combo_box ( combo_box, inp, G_N_ELEMENTS ( inp ), 0 ); scan->combo_int = combo_box; }
	if ( num == OUT_F ) { scan_append_text_combo_box ( combo_box, out, G_N_ELEMENTS ( out ), 0 ); scan->combo_out = combo_box; }

	return combo_box;
}

static GtkBox * scan_set_initial_output_file ( const char *file, uint8_t num, Scan *scan )
{
	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );

	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( entry, file );
	g_object_set ( entry, "editable", FALSE, NULL );
	gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_SECONDARY, "folder" );

	if ( num == INT_F ) scan->entry_int = entry;
	if ( num == OUT_F ) scan->entry_out = entry;

	gtk_box_pack_start ( v_box, GTK_WIDGET ( entry ), FALSE, FALSE, 0 );

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

	gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_set_initial_output_file ( "Initial file",     INT_F, scan ) ), 0, d,   2, 1 );
	gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_set_initial_output_file ( "dvb_channel.conf", OUT_F, scan ) ), 2, d++, 2, 1 );

	gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_combo_box_format ( INT_F, scan ) ), 0, d,   2, 1 );
	gtk_grid_attach ( GTK_GRID ( grid ), GTK_WIDGET ( scan_create_combo_box_format ( OUT_F, scan ) ), 2, d++, 2, 1 );
}

static void scan_init ( Scan *scan )
{
	scan_create ( scan );
}

static void scan_finalize ( GObject *object )
{
	G_OBJECT_CLASS (scan_parent_class)->finalize (object);
}

static void scan_class_init ( ScanClass *class )
{
	G_OBJECT_CLASS (class)->finalize = scan_finalize;
}

Scan * scan_new ( void )
{
	return g_object_new ( SCAN_TYPE_GRID, NULL );
}

