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

#include "dvb.h"

#include "zap.h"
#include "scan.h"
#include "level.h"
#include "control.h"

#ifndef LIGHT
  #include "settings.h"
  #include "zap-gst.h"
#endif

#include <gtk/gtk.h>

#define DVBV5_TYPE_APPLICATION    dvbv5_get_type ()
#define DVBV5_APPLICATION(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), DVBV5_TYPE_APPLICATION, Dvbv5))
#define DVBV5_IS_APPLICATION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DVBV5_TYPE_APPLICATION))
// #define DVBV5_APPLICATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  DVBV5_TYPE_APPLICATION, Dvbv5Class))
// #define DVBV5_IS_APPLICATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  DVBV5_TYPE_APPLICATION))
// #define DVBV5_APPLICATION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj),   DVBV5_TYPE_APPLICATION, Dvbv5Class))

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

	GtkPopover *popover;
	GtkNotebook *notebook;
	GtkCheckButton *toggle_fe;

	GtkLabel *dvb_name;
	GtkLabel *dvb_netw;
	GtkLabel *label_freq;
	GtkLabel *label_status;

	Zap *zap;
	Scan *scan;
	Level *level;
	Control *control;

	Dvb *dvb;

#ifndef LIGHT
	Player *player;
	Record *record;
	TcpServer *server;

	Settings *setting;
#endif

	gboolean debug;

	time_t t_dmx_start;
};

GType dvbv5_get_type ( void );

void scan_signals ( Dvbv5 * );
void zap_signals  ( Dvbv5 * );

gboolean zap_signal_parse_dvb_file ( const char *, Dvbv5 * );

void dvbv5_message_dialog ( const char *, const char *, GtkMessageType, GtkWindow * );

void dvbv5_dmx_monitor_clear ( Dvbv5 * );
void dvbv5_dmx_monitor_stop_one ( uint16_t, gboolean, gboolean, Dvbv5 * );

#endif // GTK_TYPE_APPLICATION
