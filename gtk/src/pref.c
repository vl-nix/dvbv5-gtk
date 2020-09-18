/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "pref.h"
#include "file.h"
#include "zap-signals.h"

static void pref_set_rec_dir ( GtkEntry *entry, G_GNUC_UNUSED GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEvent *event, Dvbv5 *dvbv5 )
{
	char *path = dir_open ( g_get_home_dir (), dvbv5->window );

	if ( path == NULL ) return;

	gtk_entry_set_text ( entry, path );
	free ( path );
}

static void pref_set_theme ( GtkEntry *entry, G_GNUC_UNUSED GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEvent *event, Dvbv5 *dvbv5 )
{
	char *path = dir_open ( ( g_file_test ( "/usr/share/themes", G_FILE_TEST_EXISTS ) ) ? "/usr/share/themes" : g_get_home_dir (), dvbv5->window );

	if ( path == NULL ) return;

	char *name = g_path_get_basename ( path );

	g_object_set ( gtk_settings_get_default (), "gtk-theme-name", name, NULL );

	gtk_entry_set_text ( entry, name );

	free ( name );
	free ( path );
}

static GtkEntry * pref_create_entry ( const char *text, const char *icon )
{
	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( entry, text );
	if ( icon ) gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_SECONDARY, icon );

	return entry;
}

static void pref_resize_icon ( GtkRange *range, Dvbv5 *dvbv5 )
{
	dvbv5->icon_size = gtk_range_get_value ( range );
	control_resize_icon ( dvbv5->icon_size, dvbv5->control );
}

static void pref_opacity_win ( GtkRange *range, Dvbv5 *dvbv5 )
{
	dvbv5->opacity = (uint)gtk_range_get_value ( range );
	gtk_widget_set_opacity ( GTK_WIDGET ( dvbv5->window ), (gtk_range_get_value ( range ) / 100) );
}

static GtkScale * pref_create_scale ( const char *info, uint8_t val, uint8_t min, uint8_t max, void (*f)(GtkRange *, Dvbv5 *), Dvbv5 *dvbv5 )
{
	GtkScale *scale = (GtkScale *)gtk_scale_new_with_range ( GTK_ORIENTATION_HORIZONTAL, min, max, 1 );
	// gtk_scale_set_draw_value ( scale, 0 );
	gtk_range_set_value ( GTK_RANGE ( scale ), val );
	gtk_widget_set_tooltip_text ( GTK_WIDGET ( scale ), info );

	g_signal_connect ( scale, "value-changed", G_CALLBACK ( f ), dvbv5 );

	return scale;
}

GtkBox * create_pref ( Dvbv5 *dvbv5 )
{
	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_set_spacing ( v_box, 10 );
	gtk_widget_set_margin_top   ( GTK_WIDGET ( v_box ), 10 );
	gtk_widget_set_margin_end   ( GTK_WIDGET ( v_box ), 10 );
	gtk_widget_set_margin_start ( GTK_WIDGET ( v_box ), 10 );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

	gtk_box_pack_start ( h_box, GTK_WIDGET ( gtk_label_new ( "Server" ) ), TRUE, TRUE, 0 );
	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	dvbv5->entry_host = pref_create_entry ( "127.0.0.1", NULL );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( dvbv5->entry_host ), TRUE, TRUE, 0 );

	dvbv5->entry_port = pref_create_entry ( "64000", NULL );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( dvbv5->entry_port ), TRUE, TRUE, 0 );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

	gtk_box_pack_start ( h_box, GTK_WIDGET ( gtk_label_new ( "Record" ) ), TRUE, TRUE, 0 );
	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

	dvbv5->entry_rec_dir = pref_create_entry ( g_get_home_dir (), "folder" );
	g_signal_connect ( dvbv5->entry_rec_dir, "icon-press", G_CALLBACK ( pref_set_rec_dir ), dvbv5 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( dvbv5->entry_rec_dir ), TRUE, TRUE, 0 );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

	gtk_box_pack_start ( h_box, GTK_WIDGET ( gtk_label_new ( "Theme" ) ), TRUE, TRUE, 0 );
	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

	char *theme_name = NULL;
	g_object_get ( gtk_settings_get_default (), "gtk-theme-name", &theme_name, NULL );

	dvbv5->entry_theme = pref_create_entry ( theme_name, "folder" );
	g_signal_connect ( dvbv5->entry_theme, "icon-press", G_CALLBACK ( pref_set_theme ), dvbv5 );

	free ( theme_name );

	gtk_box_pack_start ( h_box, GTK_WIDGET ( dvbv5->entry_theme ), TRUE, TRUE, 0 );
	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

	gtk_box_pack_start ( h_box, GTK_WIDGET ( pref_create_scale ( "Icon-size", dvbv5->icon_size, 8, 48,  pref_resize_icon, dvbv5 ) ), TRUE,  TRUE,  0 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( pref_create_scale ( "Opacity", dvbv5->opacity, 20, 100, pref_opacity_win, dvbv5 ) ), TRUE,  TRUE,  0 );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	return v_box;
}

#define NUM_KEYS 8

static const char *sets[NUM_KEYS][2] = 
{
	{"b", "dark"}, {"u", "opacity"}, {"u", "icon-size"}, {"s",     "host"}, 
	{"s", "port"}, {"s", "rec-dir"}, {"s",     "theme"}, {"s", "zap-conf"}
};

void load_pref ( Dvbv5 *dvbv5 )
{
	GSettingsSchemaSource *schemasrc = g_settings_schema_source_get_default ();
	GSettingsSchema *schema = g_settings_schema_source_lookup ( schemasrc, "org.gnome.dvbv5-gtk", FALSE );

	if ( schema == NULL ) g_critical ( "%s:: schema: org.gnome.dvbv5-gtk - not installed.", __func__ );
	if ( schema == NULL ) return;

	dvbv5->settings  = g_settings_new ( "org.gnome.dvbv5-gtk" );
	dvbv5->dark      = g_settings_get_boolean ( dvbv5->settings, "dark" );
	dvbv5->opacity   = g_settings_get_uint    ( dvbv5->settings, "opacity" );
	dvbv5->icon_size = g_settings_get_uint    ( dvbv5->settings, "icon-size" );

	if ( !dvbv5->debug ) return;

	uint8_t j = 0; for ( j = 0; j < NUM_KEYS; j++ )
	{
		if ( sets[j][0][0] == 'b' ) g_message ( "%s:: %s = %d ", __func__, sets[j][1], g_settings_get_boolean ( dvbv5->settings, sets[j][1] ) );
		if ( sets[j][0][0] == 'u' ) g_message ( "%s:: %s = %d ", __func__, sets[j][1], g_settings_get_uint    ( dvbv5->settings, sets[j][1] ) );
		if ( sets[j][0][0] == 's' )
		{
			char *str = g_settings_get_string ( dvbv5->settings, sets[j][1] );
			g_message ( "%s:: %s = %s ", __func__, sets[j][1], str );
			free (str);
		}
	}
}

void save_pref ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->settings == NULL ) return;

	const char *svals[NUM_KEYS] = { NULL, NULL, NULL, gtk_entry_get_text ( dvbv5->entry_host ), gtk_entry_get_text ( dvbv5->entry_port ), 
		gtk_entry_get_text ( dvbv5->entry_rec_dir ), gtk_entry_get_text ( dvbv5->entry_theme ), dvbv5->zap_file };

	const uint8_t uvals[NUM_KEYS] = { 0, dvbv5->opacity, dvbv5->icon_size, 0, 0, 0, 0 };

	uint8_t j = 0; for ( j = 0; j < NUM_KEYS; j++ )
	{
		if ( sets[j][0][0] == 'b' ) g_settings_set_boolean ( dvbv5->settings, sets[j][1], dvbv5->dark );
		if ( sets[j][0][0] == 'u' ) g_settings_set_uint    ( dvbv5->settings, sets[j][1], uvals[j] );
		if ( sets[j][0][0] == 's' ) g_settings_set_string  ( dvbv5->settings, sets[j][1], svals[j] );
	}
}

void set_pref ( Dvbv5 *dvbv5 )
{
	gtk_widget_set_opacity ( GTK_WIDGET ( dvbv5->window ), ((float)dvbv5->opacity / 100) );
	g_object_set ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", dvbv5->dark, NULL );

	if ( dvbv5->settings == NULL ) return;

	const char *sets[] = { "host", "port", "rec-dir", "theme", "zap-conf" };
	GtkEntry  *entry[] = { dvbv5->entry_host, dvbv5->entry_port, dvbv5->entry_rec_dir, dvbv5->entry_theme, NULL };

	uint8_t j = 0; for ( j = 0; j < 5; j++ )
	{
		char *data = g_settings_get_string ( dvbv5->settings, sets[j] );

		if ( entry[j] && !g_str_has_prefix ( "none", data ) ) gtk_entry_set_text ( entry[j], data );

		if ( g_str_has_prefix ( "theme", sets[j] ) && !g_str_has_prefix ( "none", data ) )
			g_object_set ( gtk_settings_get_default (), "gtk-theme-name", data, NULL );

		if ( g_str_has_prefix ( "zap-conf", sets[j] ) ) zap_signal_parse_dvb_file ( data, dvbv5 );

		free ( data );
	}
}
