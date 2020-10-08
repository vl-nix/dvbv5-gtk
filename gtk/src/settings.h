/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef SETTINGS_H
#define SETTINGS_H

#include <glib-object.h>

#define SETTINGS_TYPE_OBJECT settings_get_type ()

G_DECLARE_FINAL_TYPE (Settings, settings, SETTINGS, OBJECT, GObject)

Settings * settings_new ( void );

uint settings_get_u ( const char *, Settings * );
void settings_set_u ( uint, const char *, Settings * );

char * settings_get_s ( const char *, Settings * );
void settings_set_s ( const char *, const char *, Settings * );

gboolean settings_get_b ( const char *, Settings * );
void settings_set_b ( gboolean, const char *, Settings * );

#endif /* SETTINGS_H */
