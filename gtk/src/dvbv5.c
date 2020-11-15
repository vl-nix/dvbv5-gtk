/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "dvbv5.h"

#include <locale.h>

enum prefs
{
	PREF_HOST,
	PREF_PORT,
	PREF_RECORD,
	PREF_THEME,
	PREF_OPACITY,
	PREF_ICON_SIZE
};

#ifndef LIGHT

#endif

G_DEFINE_TYPE ( Dvbv5, dvbv5, GTK_TYPE_APPLICATION )

typedef void ( *fp  ) ( Dvbv5 *dvbv5 );
typedef void ( *fpf ) ( const char *pref, Dvbv5 *dvbv5 );

void dvbv5_message_dialog ( const char *error, const char *file_or_info, GtkMessageType mesg_type, GtkWindow *window )
{
	GtkMessageDialog *dialog = ( GtkMessageDialog *)gtk_message_dialog_new (
		window, GTK_DIALOG_MODAL, mesg_type, GTK_BUTTONS_CLOSE, "%s\n%s", error, file_or_info );

	gtk_dialog_run     ( GTK_DIALOG ( dialog ) );
	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
}

static void dvbv5_about ( Dvbv5 *dvbv5 )
{
	GtkAboutDialog *dialog = (GtkAboutDialog *)gtk_about_dialog_new ();
	gtk_window_set_transient_for ( GTK_WINDOW ( dialog ), dvbv5->window );

	GdkPixbuf *pixbuf = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default (), "dvb-logo", 48, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );

	if ( pixbuf )
	{
		gtk_window_set_icon ( GTK_WINDOW ( dialog ), pixbuf );
		gtk_about_dialog_set_logo ( dialog, pixbuf );
		g_object_unref ( pixbuf );
	}
	else
	{
		gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "display" );
		gtk_about_dialog_set_logo_icon_name ( dialog, "display" );
	}

	const char *authors[] = { "Stepan Perun", " ", NULL };
	const char *issues[]  = { "Mauro Carvalho Chehab", "zampedro", "Ro-Den", " ", NULL };

	gtk_about_dialog_add_credit_section ( dialog, "Issues ( github.com )", issues );

	gtk_about_dialog_set_program_name ( dialog, PROGNAME );
	gtk_about_dialog_set_version ( dialog, VERSION );
	gtk_about_dialog_set_license_type ( dialog, GTK_LICENSE_GPL_2_0 );
	gtk_about_dialog_set_authors ( dialog, authors );
	gtk_about_dialog_set_website ( dialog,   "https://github.com/vl-nix/dvbv5-gtk" );
	gtk_about_dialog_set_copyright ( dialog, "Copyright 2020 Dvbv5-Gtk" );
	gtk_about_dialog_set_comments  ( dialog, "Gtk+3 interface to DVBv5 tool" );

	gtk_dialog_run ( GTK_DIALOG (dialog) );
	gtk_widget_destroy ( GTK_WIDGET (dialog) );
}

static uint8_t dvbv5_get_val_togglebutton ( GtkCheckButton *checkbutton, gboolean debug )
{
	gboolean set = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON ( checkbutton ) );
	const char *name = gtk_widget_get_name  ( GTK_WIDGET ( checkbutton ) );

	uint8_t ret = ( set ) ? 1 : 0;

	if ( debug ) g_message ( "%s:: %s | set value = %d ", __func__, name, ret );

	return ret;
}

static uint8_t dvbv5_get_val_spinbutton ( GtkSpinButton *spinbutton, gboolean debug )
{
	gtk_spin_button_update ( spinbutton );

	uint8_t val = (uint8_t)gtk_spin_button_get_value_as_int ( spinbutton );
	const char *name = gtk_widget_get_name  ( GTK_WIDGET ( spinbutton ) );

	if ( debug ) g_message ( "%s:: %s | set value = %d ", __func__, name, val );

	return val;
}

static void dvbv5_stats_clear ( Dvbv5 *dvbv5 )
{
	gtk_label_set_text ( dvbv5->label_status, "" );
	gtk_label_set_text ( dvbv5->label_freq, "Freq: " );
	level_set_sgn_snr  ( 0, "Signal", "Snr", 0, 0, FALSE, dvbv5->level );
}

static void dvbv5_stats_update ( uint32_t freq, uint32_t progs, GtkLabel *label )
{
	char text[100];

	if ( progs )
		sprintf ( text, "Freq:  %d MHz  ...  [%d]", freq, progs );
	else
		sprintf ( text, "Freq:  %d MHz", freq );

	gtk_label_set_text ( label, text );
}

static uint64_t file_query_info_uint ( const char *file_path, const char *query_info, const char *attribute )
{
	GFile *file = g_file_new_for_path ( file_path );

	GFileInfo *file_info = g_file_query_info ( file, query_info, 0, NULL, NULL );

	uint64_t out = ( file_info ) ? g_file_info_get_attribute_uint64 ( file_info, attribute  ) : 0;

	g_object_unref ( file );
	if ( file_info ) g_object_unref ( file_info );

	return out;
}

#ifndef LIGHT
static void dvbv5_set_file_size ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->record == NULL ) return;

	char *file_rec = NULL;
	g_object_get ( dvbv5->record->file_sink, "location", &file_rec, NULL );

	if ( file_rec == NULL ) return;

	uint64_t dsize = file_query_info_uint ( file_rec, "standard::*", "standard::size" );

	char *str_size = g_format_size ( dsize );

	gtk_label_set_text ( dvbv5->label_status, str_size );

	g_free ( str_size );
	g_free ( file_rec );
}

void dvbv5_dmx_monitor_clear ( Dvbv5 *dvbv5 )
{
	uint8_t c = 0; for ( c = 0; c < MAX_MONITOR; c++ )
	{
		dvbv5->zap->monitor->id[c] = 0;
		dvbv5->zap->monitor->bitrate[c] = 0;
		dvbv5->zap->monitor->prw_status[c] = 0;
		dvbv5->zap->monitor->rec_status[c] = 0;
	}
}

void dvbv5_dmx_monitor_stop_one ( uint16_t num, gboolean prw, gboolean rec, Dvbv5 *dvbv5 )
{
	uint8_t c = 0; for ( c = 0; c < MAX_MONITOR; c++ )
	{
		if ( dvbv5->zap->monitor->id[c] == num )
		{
			if ( prw ) dvbv5->zap->monitor->prw_status[c] = 2;
			if ( rec ) dvbv5->zap->monitor->rec_status[c] = 2;
		}
	}
}

static void dvbv5_dmx_monitor_stop ( Dvbv5 *dvbv5 )
{
	uint8_t c = 0; for ( c = 0; c < MAX_MONITOR; c++ )
	{
		dvbv5->zap->monitor->prw_status[c] = 2;
		dvbv5->zap->monitor->rec_status[c] = 2;
	}
}

static uint dvbv5_dmx_monitor_get_bitrate ( uint16_t num, gboolean base, gboolean prw, gboolean rec, Dvbv5 *dvbv5 )
{
	if ( base )
	{
		uint bitrate = 0;
		GtkTreeModel *model = gtk_tree_view_get_model ( dvbv5->zap->treeview );
		gtk_tree_model_get ( model, &dvbv5->zap->zap_iter, COL_PRW_BITR, &bitrate, -1 );

		return bitrate;
	}

	uint32_t bitrate = 0;
	gboolean set = FALSE;

	uint8_t c = 0; for ( c = 0; c < MAX_MONITOR; c++ )
	{
		if ( dvbv5->zap->monitor->id[c] == num )
		{
			bitrate = dvbv5->zap->monitor->bitrate[c];

			if ( prw ) dvbv5->zap->monitor->prw_status[c] = 1;
			if ( rec ) dvbv5->zap->monitor->rec_status[c] = 1;

			if ( set )
			{
				dvbv5->zap->monitor->id[c] = 0;
				dvbv5->zap->monitor->bitrate[c] = 0;
				dvbv5->zap->monitor->prw_status[c] = 0;
				dvbv5->zap->monitor->rec_status[c] = 0;
			}

			set = TRUE;
		}
	}

	return bitrate;
}
#endif

static void dvbv5_show_info ( Dvbv5 *dvbv5 )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( dvbv5->zap->treeview );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );
	if ( ind == 0 ) return;

	gboolean valid;
	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid;
		  valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		gboolean toggle_rec;
		gtk_tree_model_get ( model, &iter, COL_REC, &toggle_rec, -1 );

		if ( toggle_rec )
		{
			g_autofree char *rec_file = NULL;
			gtk_tree_model_get ( model, &iter, COL_REC_FILE, &rec_file, -1 );

			if ( rec_file && g_file_test ( rec_file, G_FILE_TEST_EXISTS ) )
			{
				uint64_t dsize = file_query_info_uint ( rec_file, "standard::*", "standard::size" );

				g_autofree char *str_size = g_format_size ( dsize );

				gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_REC_SIZE, str_size, -1 );
			}
		}

#ifndef LIGHT
		gboolean toggle_prw;
		gtk_tree_model_get ( model, &iter, COL_PRW, &toggle_prw, -1 );

		uint num = 0;
		gtk_tree_model_get ( model, &iter, COL_NUM, &num, -1 );

		if ( toggle_prw || toggle_rec )
		{
			uint bitrate = dvbv5_dmx_monitor_get_bitrate ( (uint16_t)num, FALSE, toggle_prw, toggle_rec, dvbv5 );
			if ( bitrate ) gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PRW_BITR, bitrate, -1 );
		}
#endif
	}
}

static void dvbv5_set_status ( uint32_t freq, Dvbv5 *dvbv5 )
{
	dvbv5_stats_update ( freq, dvbv5->dvb->progs_scan, dvbv5->label_freq );

	time_t t_cur;
	time ( &t_cur );

	if ( ( t_cur > dvbv5->t_dmx_start ) )
	{
		time ( &dvbv5->t_dmx_start );

		if ( dvbv5->dvb->dvb_zap ) dvbv5_show_info ( dvbv5 );
	}

#ifndef LIGHT
	if ( dvbv5->zap->rec_status )
		dvbv5_set_file_size ( dvbv5 );
	else
		gtk_label_set_text ( dvbv5->label_status, "" );
#endif
}

static gboolean dvbv5_dvb_fe_show_stats ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->window == NULL ) return FALSE;

	if ( ( !dvbv5->dvb->dvb_scan && dvbv5->dvb->thread_stop ) || !dvbv5->dvb->dvb_fe )
	{
		if ( dvbv5->dvb->thread_stop && dvbv5->dvb->output_format == FILE_DVBV5 )
			zap_signal_parse_dvb_file ( dvbv5->dvb->output_file, dvbv5 );

		dvb_set_zero ( dvbv5->dvb );
		dvbv5_stats_clear ( dvbv5 );

		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->scan ), TRUE );
		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->toggle_fe ), TRUE );
		gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( dvbv5->toggle_fe ), FALSE );

		return FALSE;
	}

	uint bitrate = 0; 

#ifndef LIGHT
	if ( dvbv5->dvb->dvb_zap ) bitrate = dvbv5_dmx_monitor_get_bitrate ( 0, TRUE, FALSE, FALSE, dvbv5 );
#endif

	DvbStat dvbstat = dvb_fe_stat_get ( bitrate, dvbv5->dvb );

	if ( dvbstat.sgl || dvbstat.snr )
	{
		dvbv5_set_status ( dvbstat.freq, dvbv5 );

		level_set_sgn_snr ( (uint8_t)dvbstat.qual, ( dvbstat.sgl_str ) ? dvbstat.sgl_str : "Signal", 
			( dvbstat.snr_str ) ? dvbstat.snr_str : "Snr", (double)dvbstat.sgl, (double)dvbstat.snr, dvbstat.fe_lock, dvbv5->level );
	}
	else
	{
		dvbv5_stats_clear ( dvbv5 );
	}

	free ( dvbstat.sgl_str );
	free ( dvbstat.snr_str );

	return TRUE;
}

static void dvbv5_set_dvb ( Dvb *dvb, Dvbv5 *dvbv5 )
{
	dvb_set_zero ( dvb );

	dvb->new_freqs  = dvbv5_get_val_togglebutton ( dvbv5->scan->checkbutton[0], dvbv5->debug );
	dvb->get_detect = dvbv5_get_val_togglebutton ( dvbv5->scan->checkbutton[1], dvbv5->debug );
	dvb->get_nit    = dvbv5_get_val_togglebutton ( dvbv5->scan->checkbutton[2], dvbv5->debug );
	dvb->other_nit  = dvbv5_get_val_togglebutton ( dvbv5->scan->checkbutton[3], dvbv5->debug );

	dvb->adapter    = dvbv5_get_val_spinbutton ( dvbv5->scan->spinbutton[0], dvbv5->debug );
	dvb->frontend   = dvbv5_get_val_spinbutton ( dvbv5->scan->spinbutton[1], dvbv5->debug );
	dvb->demux      = dvbv5_get_val_spinbutton ( dvbv5->scan->spinbutton[2], dvbv5->debug );

	dvb->time_mult   = dvbv5_get_val_spinbutton ( dvbv5->scan->spinbutton[3], dvbv5->debug );
	dvb->diseqc_wait = dvbv5_get_val_spinbutton ( dvbv5->scan->spinbutton[5], dvbv5->debug );

	dvb->sat_num = (int8_t)gtk_spin_button_get_value_as_int ( dvbv5->scan->spinbutton[4] );

	if ( dvbv5->debug ) g_message ( "%s:: %s | set value = %d ", __func__, gtk_widget_get_name  ( GTK_WIDGET ( dvbv5->scan->spinbutton[4] ) ), dvb->sat_num );
	if ( dvbv5->debug ) g_message ( "%s:: %s | set value = %d ", __func__, "LNB", dvb->lnb );
	if ( dvbv5->debug ) g_message ( "%s:: %s | set value = %d ", __func__, "LNA", dvb->lna );
}

static void dvbv5_clicked_start ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->dvb->dvb_scan )
	{
		g_message ( "%s: scan-thread works ", __func__ );
		dvbv5_message_dialog ( "", "Status: works", GTK_MESSAGE_INFO, dvbv5->window );
		return;
	}

	if ( !g_file_test ( dvbv5->dvb->input_file, G_FILE_TEST_EXISTS ) )
	{
		dvbv5_message_dialog ( dvbv5->dvb->input_file, g_strerror ( errno ), GTK_MESSAGE_ERROR, dvbv5->window );
		return;
	}

	dvbv5_set_dvb ( dvbv5->dvb, dvbv5 );

	const char *error = dvb_scan ( dvbv5->dvb );

	if ( error )
	{
		dvbv5_message_dialog ( "", error, GTK_MESSAGE_ERROR, dvbv5->window );
	}
	else
	{
		if ( !gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON ( dvbv5->toggle_fe ) ) )
			gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( dvbv5->toggle_fe ), TRUE );

		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->scan ), FALSE );
		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->toggle_fe ), FALSE );
	}
}

static void dvbv5_clicked_stop ( Dvbv5 *dvbv5 )
{
#ifndef LIGHT
	dvbv5_dmx_monitor_stop ( dvbv5 );
#endif
	if ( dvbv5->window ) zap_stop_toggled_all ( dvbv5->zap );

	usleep ( 250000 );

#ifndef LIGHT
	if ( dvbv5->player ) player_stop ( dvbv5->player );
	if ( dvbv5->record ) record_stop ( dvbv5->record );
	if ( dvbv5->server ) tcpserver_stop ( dvbv5->server );
#endif

	dvb_scan_stop ( dvbv5->dvb );
	dvb_info_stop ( dvbv5->dvb );
	dvb_zap_stop  ( dvbv5->dvb );

	if ( !dvbv5->dvb->dvb_scan ) dvb_set_zero ( dvbv5->dvb );
	if ( dvbv5->window ) gtk_window_set_title ( dvbv5->window, PROGNAME );
}

static void dvbv5_info ( Dvbv5 *dvbv5 )
{
	dvbv5_about ( dvbv5 );
}

static void dvbv5_dark ( G_GNUC_UNUSED Dvbv5 *dvbv5 )
{
	gboolean dark = FALSE;
	g_object_get ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", &dark, NULL );
	g_object_set ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", !dark, NULL );

#ifndef LIGHT
	settings_set_b ( !dark, "dark", dvbv5->setting );
#endif
}

static void dvbv5_clicked_mini ( Dvbv5 *dvbv5 )
{
	if ( gtk_widget_is_visible ( GTK_WIDGET ( dvbv5->notebook ) ) )
		gtk_widget_hide ( GTK_WIDGET ( dvbv5->notebook ) );
	else
		gtk_widget_show ( GTK_WIDGET ( dvbv5->notebook ) );

	gtk_window_resize ( dvbv5->window, 300, 100 );
}

static void dvbv5_clicked_quit ( Dvbv5 *dvbv5 )
{
	gtk_widget_destroy ( GTK_WIDGET ( dvbv5->window ) );
}

static void dvbv5_button_clicked_handler ( G_GNUC_UNUSED Control *control, const char *button, Dvbv5 *dvbv5 )
{
	const char *b_n[] = { "start", "stop", "mini", "quit" };
	fp funcs[] = { dvbv5_clicked_start, dvbv5_clicked_stop, dvbv5_clicked_mini,  dvbv5_clicked_quit };

	uint8_t c = 0; for ( c = 0; c < NUM_BUTTONS; c++ ) { if ( g_str_has_prefix ( b_n[c], button ) ) { funcs[c] ( dvbv5 ); break; } }
}

static void dvbv5_set_fe ( GtkToggleButton *button, Dvbv5 *dvbv5 )
{
	gboolean set_fe = gtk_toggle_button_get_active ( button );

	if ( set_fe )
	{
		if ( dvbv5->dvb->dvb_fe ) return;

		const char *error = dvb_info ( dvbv5->dvb );

		if ( error )
		{
			gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( button ), FALSE );
			dvbv5_message_dialog ( "", error, GTK_MESSAGE_ERROR, dvbv5->window );
		}
		else
		{
			g_timeout_add ( 250, (GSourceFunc)dvbv5_dvb_fe_show_stats, dvbv5 );
		}
	}
	else
		dvb_info_stop ( dvbv5->dvb );
}

static GtkBox * dvbv5_create_info ( Dvbv5 *dvbv5 )
{
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	dvbv5->label_freq   = (GtkLabel *)gtk_label_new ( "Freq: " );
	dvbv5->label_status = (GtkLabel *)gtk_label_new ( "" );

	dvbv5->toggle_fe = (GtkCheckButton *)gtk_check_button_new_with_label ( "Fe" );
	g_signal_connect ( dvbv5->toggle_fe, "toggled", G_CALLBACK ( dvbv5_set_fe ), dvbv5 );

	gtk_box_pack_start ( h_box, GTK_WIDGET ( dvbv5->label_freq ), FALSE, FALSE, 0 );
	gtk_box_pack_end   ( h_box, GTK_WIDGET ( dvbv5->toggle_fe  ), FALSE, FALSE, 0 );
	gtk_box_pack_end   ( h_box, GTK_WIDGET ( dvbv5->label_status  ), FALSE, FALSE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( dvbv5->toggle_fe ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( dvbv5->label_freq ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( dvbv5->label_status ), TRUE );

	return h_box;
}

static GtkBox * dvbv5_create_control_box ( Dvbv5 *dvbv5 )
{
	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( dvbv5_create_info ( dvbv5 ) ), FALSE, FALSE, 0 );

	dvbv5->level = level_new ();
	gtk_box_pack_start ( v_box, GTK_WIDGET ( dvbv5->level ), FALSE, FALSE, 5 );

	dvbv5->control = control_new ();
	g_signal_connect ( dvbv5->control, "button-clicked", G_CALLBACK ( dvbv5_button_clicked_handler ), dvbv5 );
	gtk_box_pack_start ( v_box, GTK_WIDGET ( dvbv5->control ), FALSE, FALSE, 0 );

	return v_box;
}

static void dvbv5_header_bar_menu_about ( G_GNUC_UNUSED GtkButton *button, Dvbv5 *dvbv5 )
{
	gtk_widget_set_visible ( GTK_WIDGET ( dvbv5->popover ), FALSE );
	dvbv5_info ( dvbv5 );
}

static void dvbv5_header_bar_menu_dark ( G_GNUC_UNUSED GtkButton *button, Dvbv5 *dvbv5 )
{
	gtk_widget_set_visible ( GTK_WIDGET ( dvbv5->popover ), FALSE );
	dvbv5_dark ( dvbv5 );
}

static void dvbv5_header_bar_menu_quit ( G_GNUC_UNUSED GtkButton *button, Dvbv5 *dvbv5 )
{
	gtk_widget_set_visible ( GTK_WIDGET ( dvbv5->popover ), FALSE );
	gtk_widget_destroy ( GTK_WIDGET ( dvbv5->window ) );
}

#ifndef LIGHT

static void dvbv5_button_changed_record ( GtkFileChooserButton *button, Dvbv5 *dvbv5 )
{
	char *path = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( button ) );

	if ( path == NULL ) return;

	settings_set_s ( path, "rec-dir", dvbv5->setting );

	gtk_widget_set_visible ( GTK_WIDGET ( dvbv5->popover ), FALSE );

	g_free ( path );
}

static void dvbv5_button_changed_theme ( GtkFileChooserButton *button, Dvbv5 *dvbv5 )
{
	char *path = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( button ) );

	if ( path == NULL ) return;

	char *name = g_path_get_basename ( path );

	g_object_set ( gtk_settings_get_default (), "gtk-theme-name", name, NULL );

	settings_set_s ( name, "theme", dvbv5->setting );

	g_free ( name );
	g_free ( path );

	gtk_widget_set_visible ( GTK_WIDGET ( dvbv5->popover ), FALSE );
}

static GtkFileChooserButton * dvbv5_header_bar_create_chooser_button ( const char *path, const char *text_i, enum prefs prf, Dvbv5 *dvbv5 )
{
	GtkFileChooserButton *button = (GtkFileChooserButton *)gtk_file_chooser_button_new ( text_i, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER );
	gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( button ), path );
	gtk_widget_set_tooltip_text   ( GTK_WIDGET ( button ), text_i );

	if ( prf == PREF_RECORD ) g_signal_connect ( button, "file-set", G_CALLBACK ( dvbv5_button_changed_record ), dvbv5 );
	if ( prf == PREF_THEME  ) g_signal_connect ( button, "file-set", G_CALLBACK ( dvbv5_button_changed_theme  ), dvbv5 );

	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );

	return button;
}

static void dvbv5_spinbutton_changed_opacity ( GtkSpinButton *button, Dvbv5 *dvbv5 )
{
	uint opacity = (uint)gtk_spin_button_get_value_as_int ( button );

	gtk_widget_set_opacity ( GTK_WIDGET ( dvbv5->window ), ( (float)opacity / 100) );

	settings_set_u ( opacity, "opacity", dvbv5->setting );
}

static void dvbv5_spinbutton_changed_size_i ( GtkSpinButton *button, Dvbv5 *dvbv5 )
{
	uint icon_size = (uint)gtk_spin_button_get_value_as_int ( button );

	control_resize_icon ( (uint8_t)icon_size, dvbv5->control );

	settings_set_u ( icon_size, "icon-size", dvbv5->setting );
}

static GtkSpinButton * dvbv5_header_bar_create_spinbutton ( uint val, uint16_t min, uint16_t max, uint16_t step, const char *text, enum prefs prf, Dvbv5 *dvbv5 )
{
	GtkSpinButton *spinbutton = (GtkSpinButton *)gtk_spin_button_new_with_range ( min, max, step );
	gtk_spin_button_set_value ( spinbutton, val );

	const char *icon = ( control_check_icon_theme ( "dvb-info" ) ) ? "dvb-info" : "info";
	gtk_entry_set_icon_from_icon_name ( GTK_ENTRY ( spinbutton ), GTK_ENTRY_ICON_PRIMARY, icon );
	gtk_entry_set_icon_tooltip_text   ( GTK_ENTRY ( spinbutton ), GTK_ENTRY_ICON_PRIMARY, text );

	if ( prf == PREF_OPACITY   ) g_signal_connect ( spinbutton, "changed", G_CALLBACK ( dvbv5_spinbutton_changed_opacity ), dvbv5 );
	if ( prf == PREF_ICON_SIZE ) g_signal_connect ( spinbutton, "changed", G_CALLBACK ( dvbv5_spinbutton_changed_size_i  ), dvbv5 );

	gtk_widget_set_visible ( GTK_WIDGET ( spinbutton ), TRUE );

	return spinbutton;
}

#endif

static GtkPopover * dvbv5_popover_hbar ( Dvbv5 *dvbv5 )
{
	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( NULL );
	gtk_container_set_border_width ( GTK_CONTAINER ( popover ), 5 );

	GtkBox *vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 5 );
	gtk_box_set_spacing ( vbox, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( vbox ), TRUE );

#ifndef LIGHT

	g_autofree char *s_thm  = settings_get_s ( "theme",   dvbv5->setting );
	g_autofree char *s_dir  = settings_get_s ( "rec-dir", dvbv5->setting );

	uint u_opc  = settings_get_u ( "opacity", dvbv5->setting );
	uint u_isz  = settings_get_u ( "icon-size", dvbv5->setting );

	if ( s_dir && g_str_has_prefix ( "none", s_dir ) ) { free ( s_dir ); s_dir = NULL; }

	g_autofree char *name = NULL;
	g_autofree char *path = NULL;

	if ( !s_thm || ( s_thm && g_str_has_prefix ( "none", s_thm ) ) )
	{
		g_object_get ( gtk_settings_get_default (), "gtk-theme-name", &name, NULL );
		path = g_strconcat ( "/usr/share/themes/", name, NULL );
	}
	else
	{
		path = g_strconcat ( "/usr/share/themes/", s_thm, NULL );
	}

	gtk_box_pack_start ( vbox, GTK_WIDGET ( dvbv5_header_bar_create_chooser_button ( ( s_dir ) ? s_dir : g_get_home_dir (), "Record", PREF_RECORD, dvbv5 ) ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( vbox, GTK_WIDGET ( dvbv5_header_bar_create_chooser_button ( path, "Theme", PREF_THEME, dvbv5 ) ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( vbox, GTK_WIDGET ( dvbv5_header_bar_create_spinbutton ( ( u_opc ) ? u_opc : 100, 40, 100, 1, "Opacity", PREF_OPACITY, dvbv5 ) ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( vbox, GTK_WIDGET ( dvbv5_header_bar_create_spinbutton ( ( u_isz ) ? u_isz : SIZE_ICONS, 8, 48, 1, "Icon-size", PREF_ICON_SIZE, dvbv5 ) ), FALSE, FALSE, 0 );

#endif

	GtkBox *hbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( hbox, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( hbox ), TRUE );

	struct Data { const char *icon_u; const char *name; void ( *f )(GtkButton *, Dvbv5 *); } data_n[] =
	{
		{ "⏾", "dvb-dark",  dvbv5_header_bar_menu_dark  },
		{ "🛈", "dvb-info",  dvbv5_header_bar_menu_about },
		{ "⏻", "dvb-quit",  dvbv5_header_bar_menu_quit  }
	};

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( data_n ); c++ )
	{
		GtkButton *button = control_create_button ( NULL, data_n[c].name, data_n[c].icon_u, 16 );
		g_signal_connect ( button, "clicked", G_CALLBACK ( data_n[c].f ), dvbv5 );
		gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );
		gtk_box_pack_start ( hbox, GTK_WIDGET ( button ), TRUE, TRUE, 0 );
	}

	gtk_box_pack_start ( vbox, GTK_WIDGET ( hbox ), FALSE, FALSE, 0 );

	gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( vbox ) );

	return popover;
}

static GtkHeaderBar * dvbv5_header_bar ( Dvbv5 *dvbv5 )
{
	GtkHeaderBar *header_bar = (GtkHeaderBar *)gtk_header_bar_new ();
	gtk_header_bar_set_show_close_button ( header_bar, TRUE );
	gtk_header_bar_set_title ( header_bar, PROGNAME );
	gtk_header_bar_set_has_subtitle ( header_bar, FALSE );

	GtkMenuButton *menu_button = (GtkMenuButton *)gtk_menu_button_new ();
	gtk_button_set_label ( GTK_BUTTON ( menu_button ), "𝋯" );
	gtk_widget_set_visible ( GTK_WIDGET ( menu_button ), TRUE );

	dvbv5->popover = dvbv5_popover_hbar ( dvbv5 );

	gtk_menu_button_set_popover ( menu_button, GTK_WIDGET ( dvbv5->popover ) );
	gtk_header_bar_pack_end ( header_bar, GTK_WIDGET ( menu_button ) );

	return header_bar;
}


#ifndef LIGHT

static void dvbv5_set_prefs ( Dvbv5 *dvbv5 )
{
	g_autofree char *s_thm = settings_get_s ( "theme",    dvbv5->setting );
	g_autofree char *s_zcf = settings_get_s ( "zap-conf", dvbv5->setting );

	uint u_opc = settings_get_u ( "opacity", dvbv5->setting );
	uint u_isz = settings_get_u ( "icon-size", dvbv5->setting );

	gboolean dark = settings_get_b ( "dark", dvbv5->setting );

	g_object_set ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", dark, NULL );

	uint8_t set_sz = (uint8_t)( ( u_isz ) ? u_isz : SIZE_ICONS );
	control_resize_icon ( set_sz, dvbv5->control );

	if ( u_opc >= 40 ) gtk_widget_set_opacity ( GTK_WIDGET ( dvbv5->window ), ( (float)u_opc / 100) );

	if ( s_thm && !g_str_has_prefix ( "none", s_thm ) )
		g_object_set ( gtk_settings_get_default (), "gtk-theme-name", s_thm, NULL );

	if ( s_zcf && !g_str_has_prefix ( "none", s_zcf ) )
	{
		zap_signal_parse_dvb_file ( s_zcf, dvbv5 );
	}
}

#endif

static void dvbv5_set_icon_window ( GtkWindow *window )
{
	GdkPixbuf *pixbuf = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default (),
		"dvb-logo", 48, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );

	if ( pixbuf )
	{
		gtk_window_set_icon ( window, pixbuf );
		g_object_unref ( pixbuf );
	}
	else
		gtk_window_set_icon_name ( window, "display" );
}

static void dvbv5_window_quit ( G_GNUC_UNUSED GtkWindow *window, Dvbv5 *dvbv5 )
{
	dvbv5_clicked_stop ( dvbv5 );

#ifndef LIGHT

	if ( dvbv5->player ) player_destroy ( dvbv5->player );
	if ( dvbv5->record ) record_destroy ( dvbv5->record );
	if ( dvbv5->server ) tcpserver_destroy ( dvbv5->server );

	g_object_unref ( dvbv5->setting );
#endif

	dvbv5->window = NULL;

	free ( dvbv5->dvb->zap_file );
	free ( dvbv5->dvb->input_file );
	free ( dvbv5->dvb->output_file );

	free ( dvbv5->dvb );
	dvbv5->dvb = NULL;
}

static void dvbv5_new_window ( GApplication *app )
{
	setlocale ( LC_NUMERIC, "C" );

	Dvbv5 *dvbv5 = DVBV5_APPLICATION ( app );

	gtk_icon_theme_add_resource_path ( gtk_icon_theme_get_default (), "/dvbv5" );

	dvbv5->window = (GtkWindow *)gtk_application_window_new ( GTK_APPLICATION ( app ) );
	gtk_window_set_title ( dvbv5->window, PROGNAME );
	dvbv5_set_icon_window ( dvbv5->window );
	g_signal_connect ( dvbv5->window, "destroy", G_CALLBACK ( dvbv5_window_quit ), dvbv5 );

	GtkHeaderBar *header_bar = dvbv5_header_bar ( dvbv5 );
	gtk_window_set_titlebar ( dvbv5->window, GTK_WIDGET ( header_bar ) );
	gtk_widget_set_visible ( GTK_WIDGET ( header_bar ), TRUE );

	GtkBox *main_vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_set_spacing ( main_vbox, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( main_vbox ), TRUE );

	dvbv5->dvb_name = (GtkLabel *)gtk_label_new ( "Dvb Device" );
	gtk_box_pack_start ( main_vbox, GTK_WIDGET ( dvbv5->dvb_name ), FALSE, FALSE, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( dvbv5->dvb_name ), TRUE );

	dvbv5->scan = scan_new ();
	scan_signals ( dvbv5 );

	dvbv5->zap = zap_new ();
	zap_signals ( dvbv5 );

	dvbv5->notebook = (GtkNotebook *)gtk_notebook_new ();
	gtk_notebook_set_scrollable ( dvbv5->notebook, TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( dvbv5->notebook ), TRUE );

	gtk_notebook_append_page ( dvbv5->notebook, GTK_WIDGET ( dvbv5->scan ), gtk_label_new ( "Scan" ) );
	gtk_notebook_append_page ( dvbv5->notebook, GTK_WIDGET ( dvbv5->zap  ), gtk_label_new ( "Zap"  ) );

	gtk_notebook_set_tab_pos ( dvbv5->notebook, GTK_POS_TOP );
	gtk_box_pack_start ( main_vbox, GTK_WIDGET (dvbv5->notebook), TRUE, TRUE, 0 );

	GtkBox *c_box = dvbv5_create_control_box ( dvbv5 );
	gtk_box_pack_end ( main_vbox, GTK_WIDGET ( c_box ), FALSE, FALSE, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( main_vbox ), 10 );
	gtk_container_add   ( GTK_CONTAINER ( dvbv5->window ), GTK_WIDGET ( main_vbox ) );

	gtk_window_present ( dvbv5->window );

	g_autofree char *name = dvb_info_get_dvb_name ( dvbv5->dvb );

	gtk_label_set_text ( dvbv5->dvb_name, name );

#ifndef LIGHT
	dvbv5_set_prefs ( dvbv5 );
#endif

}

static void dvbv5_activate ( GApplication *app )
{
	dvbv5_new_window ( app );
}

static void dvbv5_init ( Dvbv5 *dvbv5 )
{
	dvbv5->dvb = dvb_new ();
	dvbv5->dvb->dvb_fe = NULL;
	dvbv5->dvb->dvb_zap = NULL;
	dvbv5->dvb->dvb_scan = NULL;

#ifndef LIGHT
	dvbv5->record = record_new ();
	dvbv5->player = player_new ();
	dvbv5->server = tcpserver_new ();

	dvbv5->setting = settings_new ();
#endif

	dvbv5->debug = ( g_getenv ( "DVB_DEBUG" ) ) ? TRUE : FALSE;
}

static void dvbv5_finalize ( GObject *object )
{
	G_OBJECT_CLASS (dvbv5_parent_class)->finalize (object);
}

static void dvbv5_class_init ( Dvbv5Class *class )
{
	G_APPLICATION_CLASS (class)->activate = dvbv5_activate;

	G_OBJECT_CLASS (class)->finalize = dvbv5_finalize;
}

static Dvbv5 * dvbv5_new (void)
{
	return g_object_new ( DVBV5_TYPE_APPLICATION, /*"application-id", "org.gnome.dvbv5-gtk",*/ "flags", G_APPLICATION_FLAGS_NONE, NULL );
}

int main (void)
{

#ifndef LIGHT
	gst_init ( NULL, NULL );
#endif

	Dvbv5 *app = dvbv5_new ();

	int status = g_application_run ( G_APPLICATION (app), 0, NULL );

	g_object_unref (app);

	return status;
}
