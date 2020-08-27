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

typedef gboolean bool;

typedef struct _Level Level;

struct _Level
{
	GtkBox *v_box;

	GtkLabel *sgn_snr;
	GtkProgressBar *bar_sgn;
	GtkProgressBar *bar_snr;
};

Level * level_new (void);
void level_destroy ( Level *level );
void level_set_sgn_snr ( uint8_t qual, char *sgl, char *snr, float sgl_gd, float snr_gd, bool fe_lock, Level *level );

#endif
