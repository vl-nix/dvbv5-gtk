/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef DVB_H
#define DVB_H

#include <linux/dvb/dmx.h>
#include <libdvbv5/dvb-dev.h>
#include <libdvbv5/dvb-scan.h>
#include <libdvbv5/dvb-file.h>
#include <libdvbv5/dvb-demux.h>
#include <libdvbv5/dvb-v5-std.h>

#include <glib.h>

#define MAX_AUDIO 32

typedef struct _DvbStat DvbStat;

struct _DvbStat
{
	gboolean fe_lock;
	char *sgl_str, *snr_str;
	u_int32_t qual, freq;
	double sgl, snr;
};

typedef struct _Dvb Dvb;

struct _Dvb
{
	struct dvb_device *dvb_scan, *dvb_fe, *dvb_zap;
	struct dvb_open_descriptor *pat_fd, *pmt_fd, *video_fd, *audio_fds[MAX_AUDIO];

	char *demux_dev;
	char *input_file, *output_file, *zap_file;
	enum dvb_file_formats input_format, output_format;

	u_int16_t apids[MAX_AUDIO], pids[6]; // 0 - sid, 1 - vpid, 2 - apid, 3 - apid_len, 4 - sid found, 5 - vpid found
	u_int32_t freq_scan, progs_scan;

	u_int8_t adapter, frontend, demux, time_mult, diseqc_wait;
	u_int8_t new_freqs, get_detect, get_nit, other_nit;
	int8_t lna, lnb, sat_num;

	GMutex mutex;
	GThread *thread;
	u_int8_t thread_stop;

	u_int8_t descr_num;
	u_int8_t debug;

	gboolean fe_lock;
};

Dvb * dvb_new ( void );

void dvb_set_zero ( Dvb * );

void dvb_scan_stop ( Dvb * );
const char * dvb_scan ( Dvb * );

void dvb_zap_stop ( Dvb * );
const char * dvb_zap ( const char *, u_int8_t, Dvb * );

void dvb_info_stop ( Dvb * );
const char * dvb_info ( Dvb * );

DvbStat dvb_fe_stat_get ( uint, Dvb * );
char * dvb_info_get_dvb_name ( Dvb * );
u_int8_t dvb_fe_get_is_satellite ( u_int8_t );

#endif
