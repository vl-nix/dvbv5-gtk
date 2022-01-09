/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/gpl-2.0.html
*/

#include "status.h"
#include "level.h"

#define MAX_STATS 4 // MAX_DTV_STATS

struct _Status
{
	GtkBox parent_instance;

	Level *level;

	GtkLabel *dvb_name;
	GtkLabel *freq_scan;
	GtkLabel *dvr_record;
	GtkLabel *org_status[MAX_STATS];
};

G_DEFINE_TYPE ( Status, status, GTK_TYPE_BOX )

typedef void ( *fp  ) ( GtkButton *, Status * );

static void status_handler_org ( Status *status, int num, char *text )
{
	const char *label[MAX_STATS] = { "Layer A: ", "Layer B: ","Layer C: ", "Layer D: " };

	char set_text[256];
	sprintf ( set_text, "%s  %s ", label[num], text );

	gtk_label_set_text ( status->org_status[num], set_text );
}

static void status_handler_update ( Status *status, uint32_t freq, char *fmt_size, uint8_t qual, char *sgl, char *snr, uint8_t sgl_gd, uint8_t snr_gd, gboolean fe_lock )
{
	char text[256];
	sprintf ( text, " %s ", fmt_size );

	gtk_label_set_text ( status->dvr_record, ( fmt_size ) ? text : "" );

	sprintf ( text, "Freq:  %d ", freq );

	gtk_label_set_text ( status->freq_scan, ( freq ) ? text : "" );

	g_signal_emit_by_name ( status->level, "level-update", qual, sgl, snr, sgl_gd, snr_gd, fe_lock );
}

static void status_handler_set_dvb_name ( Status *status, const char *dvb_name )
{
	gtk_label_set_text ( status->dvb_name, dvb_name );
}

static void status_clicked_scan ( G_GNUC_UNUSED GtkButton *button, Status *status )
{
	g_signal_emit_by_name ( status, "scan-start" );
}

static void status_clicked_stop ( G_GNUC_UNUSED GtkButton *button, Status *status )
{
	g_signal_emit_by_name ( status, "scan-stop" );

	gtk_label_set_text ( status->freq_scan,  "" );
	gtk_label_set_text ( status->dvr_record, "" );

	const char *label[MAX_STATS] = { "Layer A: ", "Layer B: ","Layer C: ", "Layer D: " };

	uint c = 0; for ( c = 0; c < MAX_STATS; c++ )
	{
		gtk_label_set_text ( status->org_status[c], label[c] );
	}
}

static void status_clicked_dark ( G_GNUC_UNUSED GtkButton *button, G_GNUC_UNUSED Status *status )
{
	gboolean dark = FALSE;
	g_object_get ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", &dark, NULL );
	g_object_set ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", !dark, NULL );
}

static void status_clicked_info ( G_GNUC_UNUSED GtkButton *button, Status *status )
{
	g_signal_emit_by_name ( status, "win-info" );
}

static void status_clicked_exit ( G_GNUC_UNUSED GtkButton *button, Status *status )
{
	g_signal_emit_by_name ( status, "win-close" );
}

static void status_sw_layers ( GObject *gobject, G_GNUC_UNUSED GParamSpec *pspec, Status *status )
{
	gboolean state = gtk_switch_get_state ( GTK_SWITCH ( gobject ) );

	uint c = 0; for ( c = 0; c < MAX_STATS; c++ )
	{
		gtk_widget_set_visible ( GTK_WIDGET ( status->org_status[c] ), state );
	}
}

static GtkSwitch * status_create_switch_layers ( Status *status )
{
	GtkSwitch *gswitch = (GtkSwitch *)gtk_switch_new ();
	gtk_switch_set_state ( gswitch, FALSE );
	g_signal_connect ( gswitch, "notify::active", G_CALLBACK ( status_sw_layers ), status );

	return gswitch;
}

static void status_init ( Status *status )
{
	GtkBox *box = GTK_BOX ( status );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( box ), GTK_ORIENTATION_VERTICAL );
	gtk_widget_set_visible ( GTK_WIDGET ( box ), TRUE );

	gtk_widget_set_margin_top    ( GTK_WIDGET ( box ), 10 );
	gtk_widget_set_margin_bottom ( GTK_WIDGET ( box ), 10 );
	gtk_widget_set_margin_start  ( GTK_WIDGET ( box ), 10 );
	gtk_widget_set_margin_end    ( GTK_WIDGET ( box ), 10 );

	status->dvb_name = (GtkLabel *)gtk_label_new ( "Dvb Device" );
	gtk_box_pack_start ( box, GTK_WIDGET ( status->dvb_name ), FALSE, FALSE, 0 );
	gtk_widget_set_visible (  GTK_WIDGET ( status->dvb_name ), TRUE );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );
	gtk_widget_set_visible (  GTK_WIDGET ( h_box ), TRUE );

	GtkSwitch *gswitch = status_create_switch_layers ( status );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( gswitch ), FALSE, FALSE, 0 );
	gtk_widget_set_visible (  GTK_WIDGET ( gswitch ), TRUE );

	gtk_box_pack_start ( box, GTK_WIDGET ( h_box ), FALSE, FALSE, 5 );

	const char *label[MAX_STATS] = { "Layer A: ", "Layer B: ","Layer C: ", "Layer D: " };

	uint c = 0; for ( c = 0; c < MAX_STATS; c++ )
	{
		status->org_status[c] = (GtkLabel *)gtk_label_new ( label[c] );
		gtk_widget_set_halign ( GTK_WIDGET ( status->org_status[c] ), GTK_ALIGN_START );
		gtk_box_pack_start ( box, GTK_WIDGET ( status->org_status[c] ), FALSE, FALSE, 0 );
		gtk_widget_set_visible (  GTK_WIDGET ( status->org_status[c] ), FALSE );
	}

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );
	gtk_widget_set_visible (  GTK_WIDGET ( h_box ), TRUE );

	const char *labels[] = { "dvb-start", "dvb-stop", "dvb-dark", "dvb-info", "dvb-quit" };
	// const char *labels[] = { "media-playback-start", "media-playback-stop", "stock_weather-night-clear", "info", "exit" };
	fp funcs[] = { status_clicked_scan, status_clicked_stop, status_clicked_dark, status_clicked_info, status_clicked_exit };

	for ( c = 0; c < G_N_ELEMENTS ( labels ); c++ )
	{
		GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( labels[c], GTK_ICON_SIZE_MENU );
		g_signal_connect ( button, "clicked", G_CALLBACK ( funcs[c] ), status );

		gtk_widget_set_visible (  GTK_WIDGET ( button ), TRUE );
		gtk_box_pack_start ( h_box, GTK_WIDGET ( button  ), TRUE, TRUE, 0 );
	}

	gtk_box_pack_end ( box, GTK_WIDGET ( h_box ), FALSE, FALSE, 5 );

	status->level = level_new ();
	gtk_box_pack_end ( box, GTK_WIDGET ( status->level ), FALSE, FALSE, 5 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 50 );
	gtk_widget_set_visible (  GTK_WIDGET ( h_box ), TRUE );

	gtk_box_pack_end ( box, GTK_WIDGET ( h_box ), FALSE, FALSE, 5 );

	status->freq_scan = (GtkLabel *)gtk_label_new ( "" );
	gtk_widget_set_halign ( GTK_WIDGET ( status->freq_scan ), GTK_ALIGN_START );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( status->freq_scan ), FALSE, FALSE, 0 );
	gtk_widget_set_visible (  GTK_WIDGET ( status->freq_scan ), TRUE );

	status->dvr_record = (GtkLabel *)gtk_label_new ( "" );
	gtk_widget_set_halign ( GTK_WIDGET ( status->dvr_record ), GTK_ALIGN_START );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( status->dvr_record ), FALSE, FALSE, 0 );
	gtk_widget_set_visible (  GTK_WIDGET ( status->dvr_record ), TRUE );

	g_signal_connect ( status, "set-dvb-name",  G_CALLBACK ( status_handler_set_dvb_name  ), NULL );
	g_signal_connect ( status, "status-update", G_CALLBACK ( status_handler_update ), NULL );
	g_signal_connect ( status, "status-org",    G_CALLBACK ( status_handler_org    ), NULL );	
}

static void status_finalize ( GObject *object )
{
	G_OBJECT_CLASS (status_parent_class)->finalize (object);
}

static void status_class_init ( StatusClass *class )
{
	G_OBJECT_CLASS (class)->finalize = status_finalize;

	g_signal_new ( "scan-start", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 0 );

	g_signal_new ( "scan-stop", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 0 );

	g_signal_new ( "win-info", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 0 );

	g_signal_new ( "win-close", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 0 );

	g_signal_new ( "set-dvb-name", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );

	g_signal_new ( "status-org", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING );

	g_signal_new ( "status-update", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 8, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_BOOLEAN );
}

Status * status_new (void)
{
	return g_object_new ( STATUS_TYPE_BOX, NULL );
}

