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

typedef gboolean bool;

#define NUM_BUTTONS 6

#define CONTROL_TYPE_BOX                  (control_get_type ())
G_DECLARE_FINAL_TYPE ( Control, control, CONTROL, BOX, GtkBox )

Control * control_new ( uint8_t icon_size );

void control_button_set_sensitive ( const char *name, bool set, Control *control );

#endif // CONTROL_H
