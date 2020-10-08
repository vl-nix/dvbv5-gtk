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

	dvbv5->dvb->adapter = (uint8_t)gtk_spin_button_get_value_as_int ( button );

	g_autofree char *name = dvb_info_get_dvb_name ( dvbv5->dvb );

	gtk_label_set_text ( dvbv5->dvb_name, name );
}

static void scan_signal_set_frontend ( GtkSpinButton *button, Dvbv5 *dvbv5 )
{
	gtk_spin_button_update ( button );

	dvbv5->dvb->frontend = (uint8_t)gtk_spin_button_get_value_as_int ( button );

	g_autofree char *name = dvb_info_get_dvb_name ( dvbv5->dvb );

	gtk_label_set_text ( dvbv5->dvb_name, name );
}

static void scan_signal_file_open ( GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, Dvbv5 *dvbv5 )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		const char *path = ( g_file_test ( "/usr/share/dvb/", G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR ) ) ? "/usr/share/dvb/" : g_get_home_dir ();

		char *file = file_open ( path, entry, dvbv5->window );

		if ( file ) { free ( dvbv5->dvb->input_file ); dvbv5->dvb->input_file = file; }
	}
}
static void scan_signal_file_save ( GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, Dvbv5 *dvbv5 )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		char *file = file_save ( g_get_home_dir (), entry, dvbv5->window );

		if ( file ) { free ( dvbv5->dvb->output_file ); dvbv5->dvb->output_file = file; }
	}
}

static void scan_signal_set_format_inp ( GtkComboBoxText *combo_box, Dvbv5 *dvbv5 )
{
	uint8_t inp[] = { FILE_DVBV5, FILE_CHANNEL };

	int num = gtk_combo_box_get_active ( GTK_COMBO_BOX ( combo_box ) );

	dvbv5->dvb->input_format = inp[num];
}
static void scan_signal_set_format_out ( GtkComboBoxText *combo_box, Dvbv5 *dvbv5 )
{
	uint8_t out[] = { FILE_DVBV5, FILE_VDR, FILE_CHANNEL, FILE_ZAP };

	int num = gtk_combo_box_get_active ( GTK_COMBO_BOX ( combo_box ) );

	dvbv5->dvb->output_format = out[num];
}

static void scan_signal_set_lna ( GtkComboBoxText *combo_box, Dvbv5 *dvbv5 )
{
	int8_t num = (int8_t)gtk_combo_box_get_active ( GTK_COMBO_BOX ( combo_box ) );

	dvbv5->dvb->lna = ( num == 2 ) ? -1 : num;
}

static void scan_signal_set_lnb ( GtkComboBoxText *combo_box, Dvbv5 *dvbv5 )
{
	const char *lnb_name = gtk_combo_box_get_active_id ( GTK_COMBO_BOX ( combo_box ) );

	dvbv5->dvb->lnb = (int8_t)dvb_sat_search_lnb ( lnb_name );

	if ( dvbv5->debug ) g_message ( "%s:: lnb_name %s | lnb_num = %d ", __func__, lnb_name, dvbv5->dvb->lnb );
}

static void scan_signal_clicked_button_lnb ( G_GNUC_UNUSED GtkButton *button, Dvbv5 *dvbv5 )
{
	if ( dvbv5->dvb->lnb == -1 ) return;

	const char *lnb_name = gtk_combo_box_get_active_id ( GTK_COMBO_BOX ( dvbv5->scan->combo_lnb ) );
	const char *desc = lnb_get_desc ( lnb_name );

	dvbv5_message_dialog ( lnb_name, desc, GTK_MESSAGE_INFO, dvbv5->window );
}

static void scan_signal_drag_in ( G_GNUC_UNUSED GtkGrid *grid, GdkDragContext *ct, G_GNUC_UNUSED int x, G_GNUC_UNUSED int y,
        GtkSelectionData *s_data, G_GNUC_UNUSED uint info, guint32 time, Dvbv5 *dvbv5 )
{
	char **uris = gtk_selection_data_get_uris ( s_data );

        free ( dvbv5->dvb->input_file );
        dvbv5->dvb->input_file = uri_get_path ( uris[0] );

        g_autofree char *name = g_path_get_basename ( dvbv5->dvb->input_file );

        gtk_entry_set_text ( dvbv5->scan->entry_int, name );

	g_strfreev ( uris );

	gtk_drag_finish ( ct, TRUE, FALSE, time );
}

void scan_signals ( Dvbv5 *dvbv5 )
{
	g_signal_connect ( dvbv5->scan->spinbutton[0], "changed", G_CALLBACK ( scan_signal_set_adapter  ), dvbv5 );
	g_signal_connect ( dvbv5->scan->spinbutton[1], "changed", G_CALLBACK ( scan_signal_set_frontend ), dvbv5 );

	g_signal_connect ( dvbv5->scan->entry_int, "icon-press", G_CALLBACK ( scan_signal_file_open ), dvbv5 );
	g_signal_connect ( dvbv5->scan->entry_out, "icon-press", G_CALLBACK ( scan_signal_file_save ), dvbv5 );

	g_signal_connect ( dvbv5->scan->combo_int, "changed", G_CALLBACK ( scan_signal_set_format_inp ), dvbv5 );
	g_signal_connect ( dvbv5->scan->combo_out, "changed", G_CALLBACK ( scan_signal_set_format_out ), dvbv5 );

	g_signal_connect ( dvbv5->scan->combo_lnb,  "changed", G_CALLBACK ( scan_signal_set_lnb ), dvbv5 );
	g_signal_connect ( dvbv5->scan->combo_lna,  "changed", G_CALLBACK ( scan_signal_set_lna ), dvbv5 );
	g_signal_connect ( dvbv5->scan->button_lnb, "clicked", G_CALLBACK ( scan_signal_clicked_button_lnb ), dvbv5 );

	gtk_drag_dest_set ( GTK_WIDGET ( dvbv5->scan ), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY );
	gtk_drag_dest_add_uri_targets  ( GTK_WIDGET ( dvbv5->scan ) );
	g_signal_connect ( dvbv5->scan, "drag-data-received", G_CALLBACK ( scan_signal_drag_in ), dvbv5 );
}
