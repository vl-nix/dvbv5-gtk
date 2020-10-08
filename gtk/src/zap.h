/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef ZAP_H
#define ZAP_H

#include <gtk/gtk.h>

#define MAX_MONITOR 128

typedef struct _Monitor Monitor;

struct _Monitor
{
	uint16_t id[MAX_MONITOR];
	uint32_t bitrate[MAX_MONITOR];
	uint8_t  prw_status[MAX_MONITOR];
	uint8_t  rec_status[MAX_MONITOR];
};

enum cols_n
{
	COL_NUM,
	COL_PRW,
	COL_REC,
	COL_CHAHHEL,
	COL_PRW_BITR,
	COL_REC_SIZE,
	COL_FREQ,
	COL_SID,
	COL_VPID,
	COL_APID,
	COL_REC_FILE,
	NUM_COLS
};

enum out_dmx
{
	DMX_OUT_ALL_PIDS = 4,
	DMX_OUT_OFF = 5
};

typedef struct _OutDemux OutDemux;

struct _OutDemux
{
	uint8_t descr_num;
	const char *name;
};

#define ZAP_TYPE_BOX    zap_get_type ()
#define ZAP_BOX(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), ZAP_TYPE_BOX, Zap))
#define ZAP_IS_BOX(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ZAP_TYPE_BOX))

typedef struct _Zap Zap;
typedef struct _ZapClass ZapClass;

struct _ZapClass
{
	GtkBoxClass parent_class;
};

struct _Zap
{
	GtkBox parent_instance;

	GtkTreeView *treeview;
	GtkEntry *entry_file;
	GtkComboBoxText *combo_dmx;
	GtkCheckButton *toggled_prw, *toggled_rec, *toggled_stm;

	GtkTreeIter zap_iter;

	gboolean stm_status, rec_status, prw_status;
	ulong prw_signal_id, rec_signal_id, stm_signal_id;

	char *host;
	uint16_t port;

	Monitor *monitor;
};

GType zap_get_type ( void );

Zap * zap_new ( void );

uint8_t zap_get_dmx ( uint8_t );

void zap_treeview_append ( const char *, uint16_t, uint16_t, uint16_t, uint32_t, Zap * );

void zap_stop_toggled_all ( Zap * );

#ifndef LIGHT
void zap_set_active_toggled_block ( ulong, gboolean, GtkCheckButton * );
#endif

#endif // ZAP_H
