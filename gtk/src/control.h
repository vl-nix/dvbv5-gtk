/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef CONTROL_H
#define CONTROL_H

#include <gtk/gtk.h>

#define SIZE_ICONS 20
#define NUM_BUTTONS 4

#define CONTROL_TYPE_BOX                    control_get_type ()
G_DECLARE_FINAL_TYPE ( Control, control, CONTROL, BOX, GtkBox )

Control * control_new ( void );

void control_resize_icon ( uint8_t , Control * );

void control_button_set_sensitive ( const char *, gboolean, Control * );

GtkButton * control_create_button ( GtkBox *, const char *, const char *, uint8_t );

gboolean control_check_icon_theme ( const char * );

#endif // CONTROL_H
