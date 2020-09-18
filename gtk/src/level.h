/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef LEVEL_H
#define LEVEL_H

#include <gtk/gtk.h>

#define LEVEL_TYPE_BOX                  level_get_type ()
G_DECLARE_FINAL_TYPE ( Level, level, LEVEL, BOX, GtkBox )

Level * level_new (void);

void level_set_sgn_snr ( uint8_t qual, char *sgl, char *snr, float sgl_gd, float snr_gd, gboolean fe_lock, Level *level );

#endif
