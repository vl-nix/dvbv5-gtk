/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/gpl-2.0.html
*/

#pragma once

#include <gtk/gtk.h>

char * time_to_str ( void );

char * uri_get_path ( const char * );

char * file_open ( const char *, GtkWindow * );

char * file_save ( const char *, const char *, GtkWindow * );

void dvb5_message_dialog ( const char *, const char *, GtkMessageType , GtkWindow * );

uint64_t file_query_info_uint ( const char *, const char *, const char * );

const char * dmx_rec_create ( uint8_t , uint8_t , const char *, uint8_t , uint16_t *, GtkTreeModel *, GtkTreeIter );

const char * dmx_prw_create ( uint8_t , uint8_t , const char *, uint8_t , uint16_t *, GtkTreeModel *, GtkTreeIter , const char * );

