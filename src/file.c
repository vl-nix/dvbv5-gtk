/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/gpl-2.0.html
*/

#define _LARGEFILE64_SOURCE

#define BUF_SIZE ( 8 * 128 * 188 )

#include "file.h"

#include <fcntl.h>
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <linux/dvb/dmx.h>

typedef struct _DmxRecPrw DmxRecPrw;

struct _DmxRecPrw
{
	int dmx_fd;
	int frp_fd;
	uint8_t col_num;

	char *fifo;

	GMutex mutex;

	GtkTreeIter iter;
	GtkTreeModel *model;
};

static gpointer dmx_rec_prw_thread ( DmxRecPrw *dmx_rec_prw )
{
	g_mutex_init ( &dmx_rec_prw->mutex );

	gboolean stop = FALSE;

	uint8_t buf[BUF_SIZE];
	ssize_t r = 0, w = 0;

	time_t t_start, t_cur;
	time ( &t_start );

	struct pollfd pfd;
	pfd.fd = dmx_rec_prw->dmx_fd;
	pfd.events = POLLIN | POLLPRI | POLLERR;

	while ( !stop )
	{
		int ret = poll ( &pfd, 1, 10 );

		if ( ret == -1 )
		{
			if ( errno == EINTR ) continue;

			printf ( "Demux device poll failure \n" );

			break;
		}

		if ( pfd.revents == 0 ) continue;

		r = read ( dmx_rec_prw->dmx_fd, buf, sizeof(buf) );

		if ( r < 0 )
		{
			perror ( "Read" );

			if ( errno == EOVERFLOW ) continue;

			break;
		}

		w = write ( dmx_rec_prw->frp_fd, buf, (size_t)r );

		if ( w == -1 )
		{
			if ( errno != EINTR ) { perror ( "Write rec_fd " ); break; }
		}

		time ( &t_cur );

		if ( t_cur > t_start )
		{
			g_mutex_lock ( &dmx_rec_prw->mutex );

			gboolean toggle_item;
			gtk_tree_model_get ( dmx_rec_prw->model, &dmx_rec_prw->iter, dmx_rec_prw->col_num, &toggle_item, -1 );

			if ( !toggle_item ) stop = TRUE;

			g_mutex_unlock ( &dmx_rec_prw->mutex );

			time ( &t_start );
		}
	}

	close ( dmx_rec_prw->dmx_fd );
	close ( dmx_rec_prw->frp_fd );

	if ( dmx_rec_prw->fifo ) { sleep ( 2 ); remove ( dmx_rec_prw->fifo ); free ( dmx_rec_prw->fifo ); }

	g_mutex_clear ( &dmx_rec_prw->mutex );
	free ( dmx_rec_prw );

	return NULL;
}

const char * dmx_prw_create ( uint8_t a, uint8_t d, const char *prw, uint8_t len_pid, uint16_t pids[], GtkTreeModel *model, GtkTreeIter iter, const char *player )
{
	struct dmx_pes_filter_params f;

	char dmxdev[PATH_MAX];
	sprintf ( dmxdev, "/dev/dvb/adapter%i/demux%i", a, d );

	int dmx_fd = open ( dmxdev, O_RDWR | O_NONBLOCK );

	if ( dmx_fd == -1 )
	{
		perror ( "Cannot open dmx device" );

		return "Cannot open dmx device";
	}

	if ( mkfifo ( prw, S_IRUSR | S_IWUSR ) < 0 )
	{
		perror ( "Cannot create FIFO" );
		close ( dmx_fd );

		return "Cannot create FIFO";
	}

	int prw_fd = open ( prw, O_RDWR );

	if ( prw_fd == -1 )
	{
		perror ( "Cannot open FIFO" );

		return "Cannot open FIFO";
	}

	memset(&f, 0, sizeof(f));
	f.pid = pids[0];
	f.input = DMX_IN_FRONTEND;
	f.output = DMX_OUT_TSDEMUX_TAP;
	f.pes_type = DMX_PES_OTHER;

	if ( ioctl ( dmx_fd, DMX_SET_BUFFER_SIZE, BUF_SIZE ) != 0 ) perror ( "DMX_SET_BUFFER_SIZE" );

	if ( ioctl ( dmx_fd, DMX_SET_PES_FILTER, &f ) == -1 )
	{
		perror ( "DMX_SET_PES_FILTER" );
		close ( prw_fd );
		close ( dmx_fd );
		remove ( prw );

		return "Eroor: DMX_SET_PES_FILTER";
	}

	uint8_t i = 0; for ( i = 0; i < len_pid; i++ )
	{
		if ( pids[i] == 0 ) continue;

		if ( ioctl ( dmx_fd, DMX_ADD_PID, &pids[i] ) == -1 )
		{
			perror ( "DMX_ADD_PID" );
		}
	}

	if ( ioctl ( dmx_fd, DMX_START ) == -1 )
	{
		perror ( "DMX_START" );
		close ( prw_fd );
		close ( dmx_fd );

		return "Eroor: DMX_START";
	}

	DmxRecPrw *dmx_rec_prw = g_new0 ( DmxRecPrw, 1 );

	dmx_rec_prw->dmx_fd = dmx_fd;
	dmx_rec_prw->frp_fd = prw_fd;
	dmx_rec_prw->col_num = 2 /*COL_PRW*/;
	dmx_rec_prw->fifo = g_strdup ( prw );
	dmx_rec_prw->iter = iter;
	dmx_rec_prw->model = model;

	GThread *thread = g_thread_new ( "dmx-rec_prw-thread", (GThreadFunc)dmx_rec_prw_thread, dmx_rec_prw );
	g_thread_unref ( thread );

	char play[PATH_MAX];
	sprintf ( play, "%s '%s'", player, prw );

	GError *error = NULL;
	GAppInfo *app = g_app_info_create_from_commandline ( play, NULL, G_APP_INFO_CREATE_NONE, &error );

	g_app_info_launch ( app, NULL, NULL, &error );

	if ( error )
	{
		dvb5_message_dialog ( "", error->message, GTK_MESSAGE_ERROR, NULL );
		g_error_free ( error );
	}

	g_object_unref ( app );

	return NULL;
}

const char * dmx_rec_create ( uint8_t a, uint8_t d, const char *rec, uint8_t len_pid, uint16_t pids[], GtkTreeModel *model, GtkTreeIter iter )
{
	struct dmx_pes_filter_params f;

	char dmxdev[PATH_MAX];
	sprintf ( dmxdev, "/dev/dvb/adapter%i/demux%i", a, d );

	int dmx_fd = open ( dmxdev, O_RDWR | O_NONBLOCK );

	if ( dmx_fd == -1 )
	{
		perror ( "Cannot open dmx device" );

		return "Cannot open dmx device";
	}

	int rec_fd = open ( rec, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0664 );

	if ( rec_fd == -1 )
	{
		perror ( "Cannot open rec file" );
		close ( dmx_fd );

		return "Cannot open rec file";
	}

	memset(&f, 0, sizeof(f));
	f.pid = 0;
	f.input = DMX_IN_FRONTEND;
	f.output = DMX_OUT_TSDEMUX_TAP;
	f.pes_type = DMX_PES_OTHER;

	if ( ioctl ( dmx_fd, DMX_SET_BUFFER_SIZE, BUF_SIZE ) != 0 ) perror ( "DMX_SET_BUFFER_SIZE" );

	if ( ioctl ( dmx_fd, DMX_SET_PES_FILTER, &f ) == -1 )
	{
		perror ( "DMX_SET_PES_FILTER" );
		close ( rec_fd );
		close ( dmx_fd );

		return "Eroor: DMX_SET_PES_FILTER";
	}

	uint8_t i = 0; for ( i = 0; i < len_pid; i++ )
	{
		if ( pids[i] == 0 ) continue;

		if ( ioctl ( dmx_fd, DMX_ADD_PID, &pids[i] ) == -1 )
		{
			perror ( "DMX_ADD_PID" );
		}
	}

	if ( ioctl ( dmx_fd, DMX_START ) == -1 )
	{
		perror ( "DMX_START" );
		close ( rec_fd );
		close ( dmx_fd );

		return "Eroor: DMX_START";
	}

	DmxRecPrw *dmx_rec_prw = g_new0 ( DmxRecPrw, 1 );

	dmx_rec_prw->dmx_fd = dmx_fd;
	dmx_rec_prw->frp_fd = rec_fd;
	dmx_rec_prw->col_num = 1 /*COL_REC*/;
	dmx_rec_prw->fifo = NULL;
	dmx_rec_prw->iter = iter;
	dmx_rec_prw->model = model;

	GThread *thread = g_thread_new ( "dmx-rec_prw-thread", (GThreadFunc)dmx_rec_prw_thread, dmx_rec_prw );
	g_thread_unref ( thread );

	return NULL;
}

void dvb5_message_dialog ( const char *error, const char *file_or_info, GtkMessageType mesg_type, GtkWindow *window )
{
	GtkMessageDialog *dialog = ( GtkMessageDialog *)gtk_message_dialog_new (
		window, GTK_DIALOG_MODAL, mesg_type, GTK_BUTTONS_CLOSE, "%s\n%s", error, file_or_info );

	gtk_dialog_run     ( GTK_DIALOG ( dialog ) );
	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
}

char * uri_get_path ( const char *uri )
{
	char *path = NULL;

	GFile *file = g_file_new_for_uri ( uri );

	path = g_file_get_path ( file );

	g_object_unref ( file );

	return path;
}

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

char * file_open ( const char *dir, GtkWindow *window )
{
	char *file = file_open_save ( dir, NULL, "gtk-open", "document-open", GTK_FILE_CHOOSER_ACTION_OPEN, window );

	return file;
}

char * file_save ( const char *dir, const char *file_save, GtkWindow *window )
{
	char *file = file_open_save ( dir, file_save, "gtk-save", "document-save", GTK_FILE_CHOOSER_ACTION_SAVE, window );

	return file;
}

uint64_t file_query_info_uint ( const char *file_path, const char *query_info, const char *attribute )
{
	GFile *file = g_file_new_for_path ( file_path );

	GFileInfo *file_info = g_file_query_info ( file, query_info, 0, NULL, NULL );

	uint64_t out = ( file_info ) ? g_file_info_get_attribute_uint64 ( file_info, attribute  ) : 0;

	g_object_unref ( file );
	if ( file_info ) g_object_unref ( file_info );

	return out;
}

char * time_to_str ( void )
{
	GDateTime *date = g_date_time_new_now_local ();

	char *str_time = g_date_time_format ( date, "%j-%Y-%H%M%S" );

	g_date_time_unref ( date );

	return str_time;
}

