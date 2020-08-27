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

typedef gboolean bool;

typedef struct _Zap Zap;

struct _Zap
{
	GtkBox *v_box;

	GtkTreeView *treeview;
	GtkEntry *entry_file;
	GtkComboBoxText *combo_dmx;

	bool zap_status;
};

Zap * zap_new (void);
void zap_destroy ( Zap *zap );
void zap_treeview_append ( const char *channel, uint16_t sid, uint16_t apid, uint16_t vpid, Zap *zap );

#endif // ZAP_H
