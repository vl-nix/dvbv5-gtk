/*
* Copyright 2022 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/gpl-2.0.html
*/

#pragma once

#include <gtk/gtk.h>

typedef struct _Monitor Monitor;

struct _Monitor
{
	uint8_t  column;
	uint8_t  active;
	uint32_t bitrate;
	uint64_t size_file;

	uint16_t path;
	GtkTreeView *treeview;
};

const char * dvr_rec_create ( const char *, const char *, Monitor * );

const char * dmx_rec_create ( uint8_t , uint8_t , const char *, uint8_t , uint16_t *, Monitor * );

const char * dmx_prw_create ( uint8_t , uint8_t , const char *, uint8_t , uint16_t *, Monitor *, const char * );
