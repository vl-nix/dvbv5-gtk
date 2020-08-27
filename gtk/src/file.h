/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef FILE_H
#define FILE_H

#include <gtk/gtk.h>

char * file_open ( const char *dir, GtkEntry *entry, GtkWindow *window );
char * file_save ( const char *dir, GtkEntry *entry, GtkWindow *window );

#endif // FILE_H
