/*
* Copyright 2022 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/gpl-2.0.html
*/

#define _LARGEFILE64_SOURCE

#define BUF_SIZE ( 8 * 128 * 188 )

#include "rec-prw.h"

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

	char *fifo;

	GMutex mutex;

	Monitor *monitor;
};

static void dmx_rec_prw_pid_play ( const char *file )
{
	int pid = 0, SIZE = 1024;

	char line[SIZE];

	FILE *fp = popen ( "ps -T -f -o pid:1,args", "r" );

	while ( !feof (fp) )
	{
		if ( fgets ( line, SIZE, fp ) )
		{
			if ( g_strrstr ( line, file ) ) pid = (int)strtoul ( line, NULL, 10 );
		}
	}

	pclose ( fp );

	if ( pid ) kill ( pid, SIGINT );
}

static gpointer dmx_rec_prw_thread ( DmxRecPrw *dmx_rp )
{
	g_mutex_init ( &dmx_rp->mutex );

	gboolean stop = FALSE;

	uint8_t buf[BUF_SIZE];
	ssize_t r = 0, w = 0;
	uint32_t bitrate = 0;
	uint64_t total   = 0;

	struct timespec mt1, mt2;
	clock_gettime ( CLOCK_MONOTONIC, &mt1 );

	struct pollfd pfd;
	pfd.fd = dmx_rp->dmx_fd;
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

		r = read ( dmx_rp->dmx_fd, buf, sizeof(buf) );

		if ( r < 0 )
		{
			perror ( "Read dmx_fd" );

			if ( errno == EOVERFLOW ) continue;

			break;
		}

		w = write ( dmx_rp->frp_fd, buf, (size_t)r );

		if ( w == -1 )
		{
			if ( errno != EINTR ) { perror ( "Write rec_fd " ); break; }
		}

		total += (uint32_t)r;
		bitrate += (uint32_t)r;

		clock_gettime ( CLOCK_MONOTONIC, &mt2 );

		if ( mt2.tv_sec > mt1.tv_sec )
		{
			// uint16_t t_ms = (uint16_t)( ( mt2.tv_sec * 1000 + mt2.tv_nsec / 1000000 ) - ( mt1.tv_sec * 1000 + mt1.tv_nsec / 1000000 ) );
			// g_message ( "%s:: %u msec, %u Kbps", __func__, t_ms, bitrate / 128 );

			g_mutex_lock ( &dmx_rp->mutex );

			dmx_rp->monitor->size_file = total;
			dmx_rp->monitor->bitrate = bitrate / 128;

			if ( !dmx_rp->monitor->active ) stop = TRUE;

			g_mutex_unlock ( &dmx_rp->mutex );

			bitrate = 0;
			clock_gettime ( CLOCK_MONOTONIC, &mt1 );
		}
	}

	close ( dmx_rp->dmx_fd );
	close ( dmx_rp->frp_fd );

	if ( dmx_rp->fifo ) { dmx_rec_prw_pid_play ( dmx_rp->fifo ); remove ( dmx_rp->fifo ); free ( dmx_rp->fifo ); }

	free ( dmx_rp->monitor );

	g_mutex_clear ( &dmx_rp->mutex );
	free ( dmx_rp );

	return NULL;
}

const char * dmx_prw_create ( uint8_t a, uint8_t d, const char *prw, uint8_t len_pid, uint16_t pids[], Monitor *monitor, const char *player )
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
		remove ( prw );

		return "Eroor: DMX_START";
	}

	DmxRecPrw *dmx_rp = g_new0 ( DmxRecPrw, 1 );

	dmx_rp->dmx_fd = dmx_fd;
	dmx_rp->frp_fd = prw_fd;
	dmx_rp->fifo = g_strdup ( prw );
	dmx_rp->monitor = monitor;

	GThread *thread = g_thread_new ( NULL, (GThreadFunc)dmx_rec_prw_thread, dmx_rp );
	g_thread_unref ( thread );

	char play[PATH_MAX];
	sprintf ( play, "%s '%s'", player, prw );

	GAppInfo *app = g_app_info_create_from_commandline ( play, NULL, G_APP_INFO_CREATE_NONE, NULL );

	GError *error = NULL;
	g_app_info_launch ( app, NULL, NULL, &error );

	if ( error ) { g_warning ( "%s: %s", __func__, error->message ); g_error_free ( error ); }

	g_object_unref ( app );

	return NULL;
}

const char * dmx_rec_create ( uint8_t a, uint8_t d, const char *rec, uint8_t len_pid, uint16_t pids[], Monitor *monitor )
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

	DmxRecPrw *dmx_rp = g_new0 ( DmxRecPrw, 1 );

	dmx_rp->dmx_fd = dmx_fd;
	dmx_rp->frp_fd = rec_fd;
	dmx_rp->fifo = NULL;
	dmx_rp->monitor = monitor;

	GThread *thread = g_thread_new ( NULL, (GThreadFunc)dmx_rec_prw_thread, dmx_rp );
	g_thread_unref ( thread );

	return NULL;
}

const char * dvr_rec_create ( const char *dvr, const char *rec, Monitor *monitor )
{
	int dvr_fd = open ( dvr, O_RDONLY );

	if ( dvr_fd == -1 )
	{
		perror ( "Cannot open dvr device" );

		return "Cannot open dvr device";
	}

	int rec_fd = open ( rec, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0664 );

	if ( rec_fd == -1 )
	{
		perror ( "Cannot open rec file" );
		close ( dvr_fd );

		return "Cannot open rec file";
	}

	DmxRecPrw *dmx_rp = g_new0 ( DmxRecPrw, 1 );

	dmx_rp->dmx_fd = dvr_fd;
	dmx_rp->frp_fd = rec_fd;
	dmx_rp->fifo = NULL;
	dmx_rp->monitor = monitor;

	GThread *thread = g_thread_new ( NULL, (GThreadFunc)dmx_rec_prw_thread, dmx_rp );
	g_thread_unref ( thread );

	return NULL;
}
