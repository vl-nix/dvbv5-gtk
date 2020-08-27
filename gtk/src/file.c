/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "file.h"

// Returns a newly-allocated string holding the result. Free with free()
static char * file_open_save ( const char *path, const char *file, const char *accept, const char *icon, uint8_t num, GtkWindow *window )
{
	GtkFileChooserDialog *dialog = ( GtkFileChooserDialog *)gtk_file_chooser_dialog_new (
		" ", window, num, "gtk-cancel", GTK_RESPONSE_CANCEL, accept, GTK_RESPONSE_ACCEPT, NULL );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), icon );

	gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( dialog ), path );

	if ( file )
		gtk_file_chooser_set_current_name ( GTK_FILE_CHOOSER ( dialog ), file );
	else
		gtk_file_chooser_set_select_multiple ( GTK_FILE_CHOOSER ( dialog ), FALSE );

	char *filename = NULL;

	if ( gtk_dialog_run ( GTK_DIALOG ( dialog ) ) == GTK_RESPONSE_ACCEPT )
		filename = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( dialog ) );

	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );

	return filename;
}

static void file_set_entry ( const char *file, GtkEntry *entry )
{
	char *name = g_path_get_basename ( file );
		gtk_entry_set_text ( entry, name );
	free ( name );
}

char * file_open ( const char *dir, GtkEntry *entry, GtkWindow *window )
{
	char *file = file_open_save ( dir, NULL, "gtk-open", "document-open", GTK_FILE_CHOOSER_ACTION_OPEN, window );

	if ( file == NULL ) return NULL;

	file_set_entry ( file, entry );

	return file;
}

char * file_save ( const char *dir, GtkEntry *entry, GtkWindow *window )
{
	char *file = file_open_save ( dir, "dvb_channel.conf", "gtk-save", "document-save", GTK_FILE_CHOOSER_ACTION_SAVE, window );

	if ( file == NULL ) return NULL;

	file_set_entry ( file, entry );

	return file;
}
