/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef PREF_H
#define PREF_H

#include "dvbv5.h"

GtkBox * create_pref ( Dvbv5 *dvbv5 );

void set_pref  ( Dvbv5 *dvbv5 );
void load_pref ( Dvbv5 *dvbv5 );
void save_pref ( Dvbv5 *dvbv5 );

#endif // PREF_H
