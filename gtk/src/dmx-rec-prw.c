/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#define _LARGEFILE64_SOURCE

#define BUF_SIZE ( 8 * 128 * 188 )

#include "dmx-rec-prw.h"

#ifndef LIGHT
  #include "zap-gst.h"
#endif

#include <time.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <linux/dvb/dmx.h>

typedef struct _DmxRec DmxRec;

struct _DmxRec
{
	int dmx_fd;
	int rec_fd;
	u_int16_t id;

	GMutex mutex;
	Monitor *monitor;
};

#ifndef LIGHT
typedef struct _DmxPrw DmxPrw;

struct _DmxPrw
{
	int dmx_fd;
	u_int16_t id;
	u_int16_t vpid;

	GMutex mutex;
	Monitor *monitor;
};
#endif

static gpointer dmx_rec_thread ( DmxRec *dmx_rec )
{
	g_mutex_init ( &dmx_rec->mutex );

	gboolean stop = FALSE;
	u_int8_t buf[BUF_SIZE];
	ssize_t r = 0, w = 0;

	u_int32_t total = 0;
	struct timespec mt1, mt2;
	clock_gettime ( CLOCK_MONOTONIC, &mt1 );

	u_int8_t debug = ( g_getenv ( "DVB_DEBUG" ) ) ? 1 : 0;

	struct pollfd pfd;
	pfd.fd = dmx_rec->dmx_fd;
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

		r = read ( dmx_rec->dmx_fd, buf, sizeof(buf) );

		if ( r < 0 )
		{
			perror ( "Read" );

			if ( errno == EOVERFLOW ) continue;

			printf ( "Read error \n" );
			break;
		}

		w = write ( dmx_rec->rec_fd, buf, (size_t)r );

		if ( w == -1 )
		{
			if ( errno != EINTR ) { printf ( "Write error: %m \n" ); break; }
		}

		g_mutex_lock ( &dmx_rec->mutex );

		u_int8_t c = 0; for ( c = 0; c < MAX_MONITOR; c++ )
		{
			if ( dmx_rec->monitor->id[c] == dmx_rec->id && dmx_rec->monitor->rec_status[c] == 2 )
			{
				dmx_rec->monitor->bitrate[c] = 0;
				dmx_rec->monitor->rec_status[c] = 0;

				stop = TRUE;
				break;
			}
		}

		g_mutex_unlock ( &dmx_rec->mutex );

		if ( stop ) break;

		total += (u_int32_t)r;
		clock_gettime ( CLOCK_MONOTONIC, &mt2 );

		if ( mt2.tv_sec > mt1.tv_sec )
		{
			if ( debug )
			{
				u_int16_t t_ms = (u_int16_t)( ( mt2.tv_sec * 1000 + mt2.tv_nsec / 1000000 ) - ( mt1.tv_sec * 1000 + mt1.tv_nsec / 1000000 ) );
				g_message ( "Rec:: Id %u ( %u ms ) %u Kbps", dmx_rec->id, t_ms, total / 128 );
			}

			g_mutex_lock ( &dmx_rec->mutex );

			u_int8_t c = 0; for ( c = 0; c < MAX_MONITOR; c++ )
			{
				if ( dmx_rec->monitor->id[c] == dmx_rec->id || dmx_rec->monitor->bitrate[c] == 0 )
				{
					dmx_rec->monitor->id[c] = dmx_rec->id;
					dmx_rec->monitor->bitrate[c] = total / 128;

					break;
				}
			}

			g_mutex_unlock ( &dmx_rec->mutex );

			total = 0;
			clock_gettime ( CLOCK_MONOTONIC, &mt1 );
		}
	}

	close ( dmx_rec->dmx_fd );
	close ( dmx_rec->rec_fd );

	if ( debug ) g_message ( "Rec:: Stop Id %u", dmx_rec->id );

	g_mutex_clear ( &dmx_rec->mutex );
	free ( dmx_rec );

	return NULL;
}

void dmx_rec_create ( u_int8_t a, u_int8_t d, u_int16_t id, const char *rec, u_int16_t pids[], Monitor *monitor )
{
	struct dmx_pes_filter_params f;

	char dmxdev[PATH_MAX];
	sprintf ( dmxdev, "/dev/dvb/adapter%i/demux%i", a, d );

	int dmx_fd = open ( dmxdev, O_RDWR | O_NONBLOCK );

	if ( dmx_fd == -1 )
	{
		perror ( "Cannot open dmx device" );
		return;
	}

	int rec_fd = open ( rec, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0664 );

	if ( rec_fd == -1 )
	{
		perror ( "Cannot open rec file" );
		close ( dmx_fd );
		return;
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
		close ( rec_fd );
		close ( dmx_fd );

		return;
	}

	u_int8_t i = 0; for ( i = 1; i < pids[4]; i++ )
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

		return;
	}

	DmxRec *dmx_rec = g_new0 ( DmxRec, 1 );

	dmx_rec->dmx_fd = dmx_fd;
	dmx_rec->rec_fd = rec_fd;
	dmx_rec->id = id;
	dmx_rec->monitor = monitor;

	GThread *thread = g_thread_new ( "dmx-rec-thread", (GThreadFunc)dmx_rec_thread, dmx_rec );
	g_thread_unref ( thread );
}

#ifndef LIGHT
static gpointer dmx_prw_thread ( DmxPrw *dmx_prw )
{
	g_mutex_init ( &dmx_prw->mutex );

	gboolean stop = FALSE;
	u_int8_t buf[BUF_SIZE];
	ssize_t r = 0, w = 0;

	u_int32_t total = 0;
	struct timespec mt1, mt2;
	clock_gettime ( CLOCK_MONOTONIC, &mt1 );

	u_int8_t debug = ( g_getenv ( "DVB_DEBUG" ) ) ? 1 : 0;

	int pipefd[2];
	if ( pipe ( pipefd ) == -1 )
	{
		perror ( "Pipe" );
		close ( dmx_prw->dmx_fd );
		free ( dmx_prw );
		return NULL;
	}

	PlayerPipe *player = player_pipe_new ( dmx_prw->vpid );
	player_pipe_play ( player, pipefd[0] );

	struct pollfd pfd;
	pfd.fd = dmx_prw->dmx_fd;
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

		r = read ( dmx_prw->dmx_fd, buf, sizeof(buf) );

		if ( r < 0 )
		{
			perror ( "Read" );

			if ( errno == EOVERFLOW ) continue;

			printf ( "Read error \n" );
			break;
		}

		w = write ( pipefd[1], buf, (size_t)r );

		if ( w == -1 )
		{
			if ( errno != EINTR ) { printf ( "Write error: %m \n" ); break; }
		}

		g_mutex_lock ( &dmx_prw->mutex );

		u_int8_t c = 0; for ( c = 0; c < MAX_MONITOR; c++ )
		{
			if ( dmx_prw->monitor->id[c] == dmx_prw->id && dmx_prw->monitor->prw_status[c] == 2 )
			{
				dmx_prw->monitor->bitrate[c] = 0;
				dmx_prw->monitor->prw_status[c] = 0;

				stop = TRUE;
				break;
			}
		}

		g_mutex_unlock ( &dmx_prw->mutex );

		if ( stop ) break;

		total += (u_int32_t)r;
		clock_gettime ( CLOCK_MONOTONIC, &mt2 );

		if ( mt2.tv_sec > mt1.tv_sec )
		{
			if ( debug )
			{
				u_int16_t t_ms = (u_int16_t)( ( mt2.tv_sec * 1000 + mt2.tv_nsec / 1000000 ) - ( mt1.tv_sec * 1000 + mt1.tv_nsec / 1000000 ) );
				g_message ( "Prw:: Id %u ( %u ms ) %u Kbps ", dmx_prw->id, t_ms, total / 128 );
			}

			g_mutex_lock ( &dmx_prw->mutex );

			u_int8_t c = 0; for ( c = 0; c < MAX_MONITOR; c++ )
			{
				if ( dmx_prw->monitor->id[c] == dmx_prw->id || dmx_prw->monitor->bitrate[c] == 0 )
				{
					dmx_prw->monitor->id[c] = dmx_prw->id;
					dmx_prw->monitor->bitrate[c] = total / 128;

					break;
				}
			}

			g_mutex_unlock ( &dmx_prw->mutex );

			total = 0;
			clock_gettime ( CLOCK_MONOTONIC, &mt1 );
		}
	}

	if ( player ) player_pipe_stop  ( player );

	close ( dmx_prw->dmx_fd );
	close ( pipefd[0] );
	close ( pipefd[1] );

	if ( player ) player_pipe_destroy ( player );

	if ( debug ) g_message ( "Prw:: Stop Id %u", dmx_prw->id );

	g_mutex_clear ( &dmx_prw->mutex );
	free ( dmx_prw );

	return NULL;
}

void dmx_prw_create ( u_int8_t a, u_int8_t d, u_int16_t id, u_int16_t pids[], Monitor *monitor )
{
	struct dmx_pes_filter_params f;

	char dmxdev[PATH_MAX];
	sprintf ( dmxdev, "/dev/dvb/adapter%i/demux%i", a, d );

	int dmx_fd = open ( dmxdev, O_RDWR | O_NONBLOCK );

	if ( dmx_fd == -1 )
	{
		perror ( "cannot open dmx device" );
		return;
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
		close ( dmx_fd );
		return;
	}

	u_int8_t i = 0; for ( i = 1; i < pids[4]; i++ )
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
		close ( dmx_fd );
		return;
	}

	DmxPrw *dmx_prw = g_new0 ( DmxPrw, 1 );

	dmx_prw->vpid = pids[2];
	dmx_prw->dmx_fd = dmx_fd;
	dmx_prw->id = id;
	dmx_prw->monitor = monitor;

	GThread *thread = g_thread_new ( "dmx-prw-thread", (GThreadFunc)dmx_prw_thread, dmx_prw );
	g_thread_unref ( thread );
}
#endif
