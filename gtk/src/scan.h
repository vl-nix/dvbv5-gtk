/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef SCAN_H
#define SCAN_H

#include <gtk/gtk.h>

typedef gboolean bool;

typedef struct _Scan Scan;

struct _Scan
{
	GtkGrid *grid;

	GtkSpinButton *spinbutton[6];
	GtkCheckButton *checkbutton[4];
	GtkComboBoxText *combo_lnb;
	GtkComboBoxText *combo_lna;
	GtkButton *button_lnb;
	GtkEntry *entry_int, *entry_out;
	GtkComboBoxText *combo_int, *combo_out;

	uint8_t adapter, frontend, demux, time_mult, diseqc_wait;
	uint8_t new_freqs, get_detect, get_nit, other_nit;
	int8_t lna, lnb, sat_num;
};

Scan * scan_new (void);
void scan_destroy ( Scan *scan );

#endif // SCAN_H
