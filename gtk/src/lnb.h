/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef LNB_H
#define LNB_H

#include <gtk/gtk.h>

const char * lnb_get_desc ( const char *lnb_name );
void lnb_set_name_combo ( GtkComboBoxText *combo_lnb );

#endif // LNB_H
