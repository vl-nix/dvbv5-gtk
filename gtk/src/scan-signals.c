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
#include "lnb.h"

static void scan_signal_set_adapter ( GtkSpinButton *button, Dvbv5 *dvbv5 )
{
	gtk_spin_button_update ( button );

	dvbv5->scan->adapter = gtk_spin_button_get_value_as_int ( button );

	dvbv5_get_dvb_info ( dvbv5 );
}

static void scan_signal_set_frontend ( GtkSpinButton *button, Dvbv5 *dvbv5 )
{
	gtk_spin_button_update ( button );

	dvbv5->scan->frontend = gtk_spin_button_get_value_as_int ( button );

	dvbv5_get_dvb_info ( dvbv5 );
}

static void scan_signal_set_demux ( GtkSpinButton *button, Dvbv5 *dvbv5 )
{
	gtk_spin_button_update ( button );

	dvbv5->scan->demux = gtk_spin_button_get_value_as_int ( button );
}

static void scan_signal_file_open ( GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, Dvbv5 *dvbv5 )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		const char *path = ( g_file_test ( "/usr/share/dvb/", G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR ) ) ? "/usr/share/dvb/" : g_get_home_dir ();

		char *file = file_open ( path, entry, dvbv5->window );
		if ( file ) { free ( dvbv5->input_file ); dvbv5->input_file = file; }
	}
}
static void scan_signal_file_save ( GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, Dvbv5 *dvbv5 )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		char *file = file_save ( g_get_home_dir (), entry, dvbv5->window );
		if ( file ) { free ( dvbv5->output_file ); dvbv5->output_file = file; }
	}
}

static void scan_signal_set_format_inp ( GtkComboBoxText *combo_box, Dvbv5 *dvbv5 )
{
	uint8_t inp[] = { FILE_DVBV5, FILE_CHANNEL };

	int num = gtk_combo_box_get_active ( GTK_COMBO_BOX ( combo_box ) );

	dvbv5->input_format = inp[num];
}
static void scan_signal_set_format_out ( GtkComboBoxText *combo_box, Dvbv5 *dvbv5 )
{
	uint8_t out[] = { FILE_DVBV5, FILE_VDR, FILE_CHANNEL, FILE_ZAP };

	int num = gtk_combo_box_get_active ( GTK_COMBO_BOX ( combo_box ) );

	dvbv5->output_format = out[num];
}

static void scan_signal_set_lna ( GtkComboBoxText *combo_box, Dvbv5 *dvbv5 )
{
	int num = gtk_combo_box_get_active ( GTK_COMBO_BOX ( combo_box ) );

	dvbv5->scan->lna = ( num == 2 ) ? -1 : num;
}

static void scan_signal_set_lnb ( GtkComboBoxText *combo_box, Dvbv5 *dvbv5 )
{
	const char *lnb_name = gtk_combo_box_get_active_id ( GTK_COMBO_BOX ( combo_box ) );

	dvbv5->scan->lnb = dvb_sat_search_lnb ( lnb_name );

	if ( dvbv5->debug ) g_message ( "%s:: lnb_name %s | lnb_num = %d ", __func__, lnb_name, dvbv5->scan->lnb );
}

static void scan_signal_clicked_button_lnb ( G_GNUC_UNUSED GtkButton *button, Dvbv5 *dvbv5 )
{
	if ( dvbv5->scan->lnb == -1 ) return;

	const char *lnb_name = gtk_combo_box_get_active_id ( GTK_COMBO_BOX ( dvbv5->scan->combo_lnb ) );
	const char *desc = lnb_get_desc ( lnb_name );

	dvbv5_message_dialog ( lnb_name, desc, GTK_MESSAGE_INFO, dvbv5->window );
}

void scan_signals ( Dvbv5 *dvbv5 )
{
	g_signal_connect ( dvbv5->scan->spinbutton[0], "changed", G_CALLBACK ( scan_signal_set_adapter  ), dvbv5 );
	g_signal_connect ( dvbv5->scan->spinbutton[1], "changed", G_CALLBACK ( scan_signal_set_frontend ), dvbv5 );
	g_signal_connect ( dvbv5->scan->spinbutton[2], "changed", G_CALLBACK ( scan_signal_set_demux    ), dvbv5 );

	g_signal_connect ( dvbv5->scan->entry_int, "icon-press", G_CALLBACK ( scan_signal_file_open ), dvbv5 );
	g_signal_connect ( dvbv5->scan->entry_out, "icon-press", G_CALLBACK ( scan_signal_file_save ), dvbv5 );

	g_signal_connect ( dvbv5->scan->combo_int, "changed", G_CALLBACK ( scan_signal_set_format_inp ), dvbv5 );
	g_signal_connect ( dvbv5->scan->combo_out, "changed", G_CALLBACK ( scan_signal_set_format_out ), dvbv5 );

	g_signal_connect ( dvbv5->scan->combo_lnb,  "changed", G_CALLBACK ( scan_signal_set_lnb ), dvbv5 );
	g_signal_connect ( dvbv5->scan->combo_lna,  "changed", G_CALLBACK ( scan_signal_set_lna ), dvbv5 );
	g_signal_connect ( dvbv5->scan->button_lnb, "clicked", G_CALLBACK ( scan_signal_clicked_button_lnb ), dvbv5 );
}
