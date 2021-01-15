/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef DMX_REC_H
#define DMX_REC_H

#include "zap.h"

#include <gtk/gtk.h>

#ifndef LIGHT
void dmx_prw_create ( u_int8_t, u_int8_t, u_int16_t, u_int16_t *, Monitor * );
#endif

void dmx_rec_create ( u_int8_t, u_int8_t, u_int16_t, const char *, u_int16_t *, Monitor * );

#endif // DMX_REC_H
