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

#define SCAN_TYPE_GRID    scan_get_type ()
#define SCAN_GRID(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SCAN_TYPE_GRID, Scan))
#define SCAN_IS_GRID(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SCAN_TYPE_GRID))

typedef struct _Scan Scan;
typedef struct _ScanClass ScanClass;

struct _ScanClass
{
	GtkGridClass parent_class;
};

struct _Scan
{
	GtkGrid parent_instance;

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

GType scan_get_type (void);

Scan * scan_new (void);

#endif // SCAN_H
