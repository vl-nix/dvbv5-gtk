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

enum cols_n
{
	COL_NUM,
	COL_CHAHHEL,
	COL_SID,
	COL_VIDEO_PID,
	COL_AUDIO_PID,
	NUM_COLS
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
	GtkCheckButton *toggled_prw, *toggled_rec, *toggled_stm, *toggled_tmo;

	gboolean zap_status;
	gboolean stm_status, rec_status, prw_status;

	ulong prw_signal_id, rec_signal_id, stm_signal_id, tmo_signal_id;
};

GType zap_get_type (void);

Zap * zap_new (void);

void zap_stop_toggled_all ( Zap *zap );
void zap_set_active_toggled_block ( ulong signal_id, gboolean active, GtkCheckButton *toggle );
void zap_treeview_append ( const char *channel, uint16_t sid, uint16_t apid, uint16_t vpid, Zap *zap );

#endif // ZAP_H
