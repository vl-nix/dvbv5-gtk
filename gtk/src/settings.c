/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "settings.h"

#include <inttypes.h>
#include <gio/gio.h>

/*
static const char *sets[8][2] = 
{
	{"b", "dark"}, {"u", "opacity"}, {"u", "icon-size"}, {"s",     "host"}, 
	{"s", "port"}, {"s", "rec-dir"}, {"s",     "theme"}, {"s", "zap-conf"}
};
*/

struct _Settings
{
	GObject parent_instance;

	GSettings *setting;
};

G_DEFINE_TYPE (Settings, settings, G_TYPE_OBJECT)

uint settings_get_u ( const char *str, Settings *obj )
{
	if ( obj->setting == NULL ) return 0;

	return g_settings_get_uint ( obj->setting, str );
}

void settings_set_u ( uint val, const char *str, Settings *obj )
{
	if ( obj->setting == NULL ) return;

	g_settings_set_uint ( obj->setting, str, val );
}

char * settings_get_s ( const char *str, Settings *obj )
{
	if ( obj->setting == NULL ) return NULL;

	return g_settings_get_string ( obj->setting, str );
}

void settings_set_s ( const char *val, const char *str, Settings *obj )
{
	if ( obj->setting == NULL ) return;

	g_settings_set_string ( obj->setting, str, val );
}

gboolean settings_get_b ( const char *str, Settings *obj )
{
	if ( obj->setting == NULL ) return 0;

	return g_settings_get_boolean ( obj->setting, str );
}

void settings_set_b ( gboolean val, const char *str, Settings *obj )
{
	if ( obj->setting == NULL ) return;

	g_settings_set_boolean ( obj->setting, str, val );
}

static void settings_init ( Settings *obj )
{
	GSettingsSchemaSource *schemasrc = g_settings_schema_source_get_default ();
	GSettingsSchema *schema = g_settings_schema_source_lookup ( schemasrc, "org.gnome.dvbv5-gtk", FALSE );

	obj->setting = NULL;

	if ( schema == NULL ) g_critical ( "%s:: schema: org.gnome.dvbv5-gtk - not installed.", __func__ );
	if ( schema == NULL ) return;

	obj->setting = g_settings_new ( "org.gnome.dvbv5-gtk" );
}

static void settings_finalize ( GObject *object )
{
	G_OBJECT_CLASS (settings_parent_class)->finalize (object);
}

static void settings_class_init ( SettingsClass *class )
{
	G_OBJECT_CLASS (class)->finalize = settings_finalize;
}

Settings * settings_new ( void )
{
	return g_object_new ( SETTINGS_TYPE_OBJECT, NULL );
}
