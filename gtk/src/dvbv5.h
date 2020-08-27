/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef DVBV5_H
#define DVBV5_H

#include "zap.h"
#include "scan.h"
#include "level.h"
#include "control.h"

#include <linux/dvb/dmx.h>
#include <libdvbv5/dvb-dev.h>
#include <libdvbv5/dvb-scan.h>
#include <libdvbv5/dvb-file.h>
#include <libdvbv5/dvb-demux.h>
#include <libdvbv5/dvb-v5-std.h>

#include <gtk/gtk.h>

#define MAX_AUDIO 32

#define DVBV5_TYPE_APPLICATION             (dvbv5_get_type ())
#define DVBV5_APPLICATION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), DVBV5_TYPE_APPLICATION, Dvbv5))
#define DVBV5_APPLICATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  DVBV5_TYPE_APPLICATION, Dvbv5Class))
#define DVBV5_IS_APPLICATION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DVBV5_TYPE_APPLICATION))
#define DVBV5_IS_APPLICATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  DVBV5_TYPE_APPLICATION))

typedef gboolean bool;
typedef struct _Dvbv5 Dvbv5;
typedef struct _Dvbv5Class Dvbv5Class;

struct _Dvbv5Class
{
	GtkApplicationClass parent_class;
};

struct _Dvbv5
{
	GtkApplication parent_instance;

	GtkWindow *window;
	GtkNotebook *notebook;
	GtkLabel *dvb_name, *label_freq;
	GtkCheckButton *toggle_fe;

	GMutex mutex;
	GThread *thread;
	bool thread_stop, thread_save, stat_stop;

	Zap *zap;
	Scan *scan;
	Level *level;
	Control *control;

	struct dvb_device *dvb_scan, *dvb_fe, *dvb_zap;
	struct dvb_open_descriptor *pat_fd, *pmt_fd, *video_fd, *audio_fds[MAX_AUDIO];

	char *demux_dev;
	char *input_file, *output_file, *zap_file;
	enum dvb_file_formats input_format, output_format;

	uint16_t apids[MAX_AUDIO], pids[6]; // 0 - sid, 1 - vpid, 2 - apid, 3 - apid_len, 4 - sid found, 5 - vpid found
	uint32_t freq_scan, progs_scan, cookie;

	bool fe_lock, debug;
};

GType dvbv5_get_type (void);

void dvbv5_get_dvb_info ( Dvbv5 *dvbv5 );
bool _dvbv5_dvb_zap_init ( const char *channel, Dvbv5 *dvbv5 );
void dvbv5_message_dialog ( const char *error, const char *file_or_info, GtkMessageType mesg_type, GtkWindow *window );

#endif // GTK_TYPE_APPLICATION
