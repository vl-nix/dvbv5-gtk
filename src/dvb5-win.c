/*
* Copyright 2022 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/gpl-2.0.html
*/

#include "dvb.h"
#include "level.h"
#include "rec-prw.h"
#include "dvb5-win.h"

#include <locale.h>
#include <linux/dvb/dmx.h>

#define MAX_STATS 4 // MAX_DTV_STATS
#define DMX_OUT_ALL_PIDS 4

enum col_tree
{
	COL_NUM,
	COL_REC,
	COL_PRW,
	COL_CHL,
	COL_SIZE,
	COL_SID,
	COL_VPID,
	COL_APID,
	NUM_COLS
};

typedef struct _OutDemux OutDemux;

struct _OutDemux
{
	uint8_t descr_num;
	const char *name;
};

const OutDemux out_demux_n[] =
{
	{ DMX_OUT_DECODER, 		"DMX_OUT_DECODER" 	  },
	{ DMX_OUT_TAP, 			"DMX_OUT_TAP"		  },
	{ DMX_OUT_TS_TAP, 		"DMX_OUT_TS_TAP" 	  },
	{ DMX_OUT_TSDEMUX_TAP, 	"DMX_OUT_TSDEMUX_TAP" },
	{ DMX_OUT_ALL_PIDS, 	"DMX_OUT_ALL_PIDS" 	  }
};

typedef struct _DvbLnb DvbLnb;

struct _DvbLnb
{
	const char *abr;
	const char *name;
	const char *desc;
};

const DvbLnb dvb_lnb_type_n[] =
{
	{ "EXT",   "EXTENDED", "Freqs: 10700 to 11700 MHz, LO: 9750 MHz\nFreqs: 11700 to 12750 MHz, LO: 10600 MHz" },
	{ "UNV",   "UNIVERSAL", "Freqs: 10800 to 11800 MHz, LO: 9750 MHz\nFreqs: 11600 to 12700 MHz, LO: 10600 MHz" },
	{ "DBS",   "DBS", "Freqs: 12200 to 12700 MHz, LO: 11250 MHz" },
	{ "STD",   "STANDARD", "Freqs: 10945 to 11450 MHz, LO: 10000 MHz" },
	{ "L1070", "L10700", "Freqs: 11750 to 12750 MHz, LO: 10700 MHz" },
	{ "L1075", "L10750", "Freqs: 11700 to 12200 MHz, LO: 10750 MHz" },
	{ "L1130", "L11300", "Freqs: 12250 to 12750 MHz, LO: 11300 MHz" },
	{ "ENH",   "ENHANCED", "Freqs: 10700 to 11700 MHz, LO: 9750 MHz" },
	{ "QPH",   "QPH031", "Freqs: 11700 to 12200 MHz, LO: 10750 MHz\nFreqs: 12200 to 12700 MHz, LO: 11250 MHz" },
	{ "CBD",   "C-BAND", "Freqs: 3700 to 4200 MHz, LO: 5150 MHz" },
	{ "CMT",   "C-MULT", "Right: 3700 to 4200 MHz, LO: 5150 MHz\nLeft: 3700 to 4200 MHz, LO: 5750 MHz" },
	{ "DSH",   "DISHPRO", "Vertical: 12200 to 12700 MHz, LO: 11250 MHz\nHorizontal: 12200 to 12700 MHz, LO: 14350 MHz" },
	{ "BSJ",   "110BS", "Freqs: 11710 to 12751 MHz, LO: 10678 MHz" },
	{ "STBRS", "STACKED-BRASILSAT", "Horizontal: 10700 to 11700 MHz, LO: 9710 MHz\nHorizontal: 10700 to 11700 MHz, LO: 9750 MHz" },
	{ "OIBRS", "OI-BRASILSAT", "Freqs: 10950 to 11200 MHz, LO: 10000 MHz\nFreqs: 11800 to 12200 MHz, LO: 10445 MHz" },
	{ "AMZ3",  "AMAZONAS", "Vertical: 11037 to 11450 MHz, LO: 9670 MHz\nHorizontal: 11770 to 12070 MHz, LO: 9922 MHz\nHorizontal: 10950 to 11280 MHz, LO: 10000 MHz" },
	{ "AMZ2",  "AMAZONAS", "Vertical: 11037 to 11360 MHz, LO: 9670 MHz\nHorizontal: 11780 to 12150 MHz, LO: 10000 MHz\nHorizontal: 10950 to 11280 MHz, LO: 10000 MHz" },
	{ "GVBRS", "GVT-BRASILSAT", "Vertical: 11010 to 11067 MHz, LO: 12860 MHz\nVertical: 11704 to 11941 MHz, LO: 13435 MHz\nHorizontal: 10962 to 11199 MHz, LO: 13112 MHz\nHorizontal: 11704 to 12188 MHz, LO: 13138 MHz" }
};

struct _Dvb5Win
{
	GtkWindow  parent_instance;
	GtkNotebook *notebook;

	GtkEntry *entry_int;
	GtkEntry *entry_out;
	GtkComboBoxText *combo_lnb;
	GtkComboBoxText *combo_lna;
	GtkComboBoxText *combo_int;
	GtkComboBoxText *combo_out;

	GtkEntry *entry_rec;
	GtkEntry *entry_play;
	GtkEntry *entry_file;
	GtkTreeView *treeview;
	GtkComboBoxText *combo_dmx;

	Level *level;
	GtkLabel *dvr_rec;
	GtkLabel *dvb_name;
	GtkLabel *freq_scan;
	GtkLabel *org_status[MAX_STATS];

	Monitor *monitor_dvr;
	gboolean stop_dvr_rec;

	Dvb *dvb;
	gboolean fe_lock;

	int8_t  sat_num; // lna, lnb;
	uint8_t new_freqs, get_detect, get_nit, other_nit;
	uint8_t adapter, frontend, demux, time_mult, diseqc_wait;
};

G_DEFINE_TYPE ( Dvb5Win, dvb5_win, GTK_TYPE_WINDOW )

typedef void ( *fp  ) ( GtkButton *, Dvb5Win * );

static void dvbv5_about ( Dvb5Win *win )
{
	GtkAboutDialog *dialog = (GtkAboutDialog *)gtk_about_dialog_new ();
	gtk_window_set_transient_for ( GTK_WINDOW ( dialog ), GTK_WINDOW ( win ) );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "dvbv5-gtk" );
	gtk_about_dialog_set_logo_icon_name ( dialog, "dvbv5-gtk" );

	const char *authors[] = { "Stepan Perun", " ", NULL };
	const char *issues[]  = { "Mauro Carvalho Chehab", "zampedro", "Ro-Den", " ", NULL };

	gtk_about_dialog_add_credit_section ( dialog, "Issues ( github.com )", issues );

	gtk_about_dialog_set_program_name ( dialog, "Dvbv5-Gtk" );
	gtk_about_dialog_set_version ( dialog, VERSION );
	gtk_about_dialog_set_license_type ( dialog, GTK_LICENSE_GPL_2_0 );
	gtk_about_dialog_set_authors ( dialog, authors );
	gtk_about_dialog_set_website ( dialog,   "https://github.com/vl-nix/dvbv5-gtk" );
	gtk_about_dialog_set_copyright ( dialog, "Copyright 2022 Dvbv5-Gtk" );
	gtk_about_dialog_set_comments  ( dialog, "Gtk+3 interface to DVBv5 tool" );

	gtk_dialog_run ( GTK_DIALOG (dialog) );
	gtk_widget_destroy ( GTK_WIDGET (dialog) );
}

static void dvb5_message_dialog ( const char *error, const char *file_or_info, GtkMessageType mesg_type, GtkWindow *window )
{
	GtkMessageDialog *dialog = ( GtkMessageDialog *)gtk_message_dialog_new ( window, GTK_DIALOG_MODAL, mesg_type, GTK_BUTTONS_CLOSE, "%s\n%s", error, file_or_info );

	gtk_dialog_run     ( GTK_DIALOG ( dialog ) );
	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
}

static char * file_open_save ( const char *path, const char *file, const char *accept, const char *icon, uint8_t num, GtkWindow *window )
{
	GtkFileChooserDialog *dialog = ( GtkFileChooserDialog *)gtk_file_chooser_dialog_new ( " ", window, num, "gtk-cancel", GTK_RESPONSE_CANCEL, accept, GTK_RESPONSE_ACCEPT, NULL );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), icon );

	gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( dialog ), path );

	if ( file )
		gtk_file_chooser_set_current_name ( GTK_FILE_CHOOSER ( dialog ), file );
	else
		gtk_file_chooser_set_select_multiple ( GTK_FILE_CHOOSER ( dialog ), FALSE );

	char *filename = NULL;

	if ( gtk_dialog_run ( GTK_DIALOG ( dialog ) ) == GTK_RESPONSE_ACCEPT ) filename = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( dialog ) );

	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );

	return filename;
}

static char * file_open ( const char *dir, gboolean set_dir, GtkWindow *window )
{
	return file_open_save ( dir, NULL, "gtk-open", "document-open", ( set_dir ) ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER : GTK_FILE_CHOOSER_ACTION_OPEN, window );
}

static char * file_save ( const char *dir, const char *file_save, GtkWindow *window )
{
	return file_open_save ( dir, file_save, "gtk-save", "document-save", GTK_FILE_CHOOSER_ACTION_SAVE, window );
}

static void set_margin ( uint8_t margin, GtkWidget *widget )
{
	gtk_widget_set_margin_top    ( widget, margin );
	gtk_widget_set_margin_bottom ( widget, margin );
	gtk_widget_set_margin_start  ( widget, margin );
	gtk_widget_set_margin_end    ( widget, margin );
}

// ***** Scan *****

static void scan_signal_spin ( GtkSpinButton *spin, Dvb5Win *win )
{
	int val = gtk_spin_button_get_value_as_int ( spin );
	const char *name = gtk_widget_get_name ( GTK_WIDGET ( spin ) );

	if ( g_str_equal      ( name, "DISEqC"   ) ) win->sat_num     = (int8_t)val;
	if ( g_str_has_prefix ( name, "Adapter"  ) ) win->adapter     = (uint8_t)val;
	if ( g_str_has_prefix ( name, "Frontend" ) ) win->frontend    = (uint8_t)val;
	if ( g_str_has_prefix ( name, "Demux"    ) ) win->demux       = (uint8_t)val;
	if ( g_str_has_prefix ( name, "Timeout"  ) ) win->time_mult   = (uint8_t)val;
	if ( g_str_has_prefix ( name, "Wait"     ) ) win->diseqc_wait = (uint8_t)val;

	if ( g_str_has_prefix ( name, "Adapter" ) || g_str_has_prefix ( name, "Frontend" ) ) g_signal_emit_by_name ( win->dvb, "dvb-info", win->adapter, win->frontend );

	g_debug ( "%s: %s = %d ", __func__, name, val );
}

static void scan_signal_toggled ( GtkToggleButton *toggled, Dvb5Win *win )
{
	gboolean val = gtk_toggle_button_get_active ( toggled );
	const char *name = gtk_widget_get_name ( GTK_WIDGET ( toggled ) );

	if ( g_str_has_prefix ( name, "Freqs-only"   ) ) win->new_freqs  = ( val ) ? 1 : 0;
	if ( g_str_has_prefix ( name, "Get frontend" ) ) win->get_detect = ( val ) ? 1 : 0;
	if ( g_str_has_prefix ( name, "Nit"          ) ) win->get_nit    = ( val ) ? 1 : 0;
	if ( g_str_has_prefix ( name, "Other nit"    ) ) win->other_nit  = ( val ) ? 1 : 0;

	g_debug ( "%s: %s = %d ", __func__, name, val );
}

static GtkLabel * scan_create_label ( const char *text )
{
	GtkLabel *label = (GtkLabel *)gtk_label_new ( text );

	gtk_widget_set_halign ( GTK_WIDGET ( label ), GTK_ALIGN_START );
	gtk_widget_set_visible ( GTK_WIDGET ( label ), TRUE );

	return label;
}

static GtkWidget * scan_create_spin ( int16_t min, int16_t max, uint8_t step, int16_t value, const char *name, Dvb5Win *win )
{
	GtkSpinButton *spinbutton = (GtkSpinButton *)gtk_spin_button_new_with_range ( min, max, step );

	gtk_spin_button_set_value ( spinbutton, value );
	gtk_widget_set_name ( GTK_WIDGET ( spinbutton ), name );
	gtk_widget_set_visible ( GTK_WIDGET ( spinbutton ), TRUE );

	g_signal_connect ( spinbutton, "value-changed", G_CALLBACK ( scan_signal_spin ), win );

	return GTK_WIDGET ( spinbutton );
}

static GtkWidget * scan_create_check ( gboolean act, const char *name, Dvb5Win *win )
{
	GtkCheckButton *checkbutton = (GtkCheckButton *)gtk_check_button_new ();

	gtk_widget_set_name ( GTK_WIDGET ( checkbutton ), name );
	gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( checkbutton ), act );
	gtk_widget_set_visible ( GTK_WIDGET ( checkbutton ), TRUE );

	g_signal_connect ( checkbutton, "toggled", G_CALLBACK ( scan_signal_toggled ), win );

	return GTK_WIDGET ( checkbutton );
}

static void scan_append_text_combo_box ( GtkComboBoxText *combo_box, const char *text[], uint8_t indx, int8_t active )
{
	uint8_t c = 0; for ( c = 0; c < indx; c++ )
		gtk_combo_box_text_append_text ( combo_box, text[c] );

	gtk_combo_box_set_active ( GTK_COMBO_BOX ( combo_box ), active );
}

static GtkWidget * scan_create_combo_lna ( const char *name, Dvb5Win *win )
{
	const char *lna[] = { "On", "Off", "Auto" };

	GtkComboBoxText *combo = (GtkComboBoxText *) gtk_combo_box_text_new ();
	gtk_widget_set_name ( GTK_WIDGET ( combo ), name );
	scan_append_text_combo_box ( combo, lna, G_N_ELEMENTS ( lna ), 2 );

	gtk_widget_set_visible ( GTK_WIDGET ( combo ), TRUE );

	win->combo_lna = combo;

	return GTK_WIDGET ( combo );
}

static void scan_signal_clicked_button_lnb ( G_GNUC_UNUSED GtkButton *button, Dvb5Win *win )
{
	const char *lnb_name = gtk_combo_box_get_active_id ( GTK_COMBO_BOX ( win->combo_lnb ) );

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( dvb_lnb_type_n ); c++ )
	{
		if ( g_str_equal ( dvb_lnb_type_n[c].name, lnb_name ) ) { dvb5_message_dialog ( lnb_name, dvb_lnb_type_n[c].desc, GTK_MESSAGE_INFO, GTK_WINDOW ( win ) ); break; }
	}
}

static GtkWidget * scan_create_combo_lnb ( const char *name, Dvb5Win *win )
{
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkComboBoxText *combo = (GtkComboBoxText *) gtk_combo_box_text_new ();
	gtk_widget_set_name ( GTK_WIDGET ( combo ), name );
	gtk_combo_box_text_append ( combo, "NONE", "None" );

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( dvb_lnb_type_n ); c++ )
	{
		const char *abr  = dvb_lnb_type_n[c].abr;
		const char *name = dvb_lnb_type_n[c].name;

		gtk_combo_box_text_append ( combo, name, abr );
	}

	gtk_combo_box_set_active ( GTK_COMBO_BOX ( combo ), 0 );

	GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( "dvb-info", GTK_ICON_SIZE_MENU );
	g_signal_connect ( button, "clicked", G_CALLBACK ( scan_signal_clicked_button_lnb ), win );

	gtk_box_pack_start ( h_box, GTK_WIDGET ( combo  ), TRUE, TRUE, 0 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( button ), TRUE, TRUE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( combo  ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );

	win->combo_lnb = combo;

	return GTK_WIDGET ( h_box );
}

static void scan_signal_entry ( GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, GtkPopover *popover )
{
	const char *name = gtk_widget_get_name ( GTK_WIDGET ( entry ) );

	if ( icon_pos == GTK_ENTRY_ICON_PRIMARY )
	{
		cairo_rectangle_int_t rect;

		gtk_entry_get_icon_area ( entry, icon_pos, &rect );
		gtk_popover_set_pointing_to ( popover, &rect );
		gtk_popover_popup ( popover );

		g_object_set_data ( G_OBJECT (entry), "popover-icon-pos", GUINT_TO_POINTER (icon_pos) );
	}

	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		g_autofree char *file = NULL;

		if ( g_str_has_prefix ( name, "Open" ) )
		{
			GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( entry ) ) );

			const char *path = ( g_file_test ( "/usr/share/dvb/", G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR ) ) ? "/usr/share/dvb/" : g_get_home_dir ();

			file = file_open ( path, FALSE, window );

			if ( file ) gtk_entry_set_text ( entry, file );
		}

		if ( g_str_has_prefix ( name, "Save" ) )
		{
			GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( entry ) ) );

			file = file_save ( g_get_home_dir (), "dvb_channel.conf", window );

			if ( file ) gtk_entry_set_text ( entry, file );
		}
	}
}

static GtkComboBoxText * scan_create_popover ( const char *name, Dvb5Win *win )
{
	const char *inp[] = { "DVBV5", "CHANNEL" };
	const char *out[] = { "DVBV5", "VDR", "CHANNEL", "ZAP" };

	GtkComboBoxText *combo_box = (GtkComboBoxText *) gtk_combo_box_text_new ();
	gtk_widget_set_name ( GTK_WIDGET ( combo_box ), name );

	if ( g_str_has_prefix ( name, "Open" ) ) { win->combo_int = combo_box; scan_append_text_combo_box ( combo_box, inp, G_N_ELEMENTS ( inp ), 0 ); }
	if ( g_str_has_prefix ( name, "Save" ) ) { win->combo_out = combo_box; scan_append_text_combo_box ( combo_box, out, G_N_ELEMENTS ( out ), 0 ); }

	gtk_widget_set_visible ( GTK_WIDGET ( combo_box ), TRUE );

	return combo_box;
}

static GtkWidget * scan_create_in_out_entry ( const char *file, const char *name, Dvb5Win *win )
{
	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( entry, file );
	g_object_set ( entry, "editable", FALSE, NULL );
	gtk_widget_set_name ( GTK_WIDGET ( entry ), name );

	gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_PRIMARY, "dvb-list" );
	gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_SECONDARY, "folder" );

	gtk_widget_set_visible ( GTK_WIDGET ( entry ), TRUE );

	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( GTK_WIDGET ( entry ) );
	gtk_widget_set_name ( GTK_WIDGET ( popover ), name );
	gtk_popover_set_position ( popover, GTK_POS_TOP );
	gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( scan_create_popover ( name, win ) ) );
	gtk_container_set_border_width ( GTK_CONTAINER ( popover ), 2 );

	if ( g_str_has_prefix ( name, "Open" ) ) win->entry_int = entry;
	if ( g_str_has_prefix ( name, "Save" ) ) win->entry_out = entry;

	g_signal_connect ( entry, "icon-press", G_CALLBACK ( scan_signal_entry ), popover );

	return GTK_WIDGET ( entry );
}

static void scan_signal_drag_in ( G_GNUC_UNUSED GtkGrid *grid, GdkDragContext *ct, G_GNUC_UNUSED int x, G_GNUC_UNUSED int y, GtkSelectionData *s_data, G_GNUC_UNUSED uint info, guint32 time, Dvb5Win *win )
{
	char **uris = gtk_selection_data_get_uris ( s_data );

	g_autofree char *file = g_filename_from_uri ( uris[0], NULL, NULL );

	gtk_entry_set_text ( win->entry_int, file );

	g_strfreev ( uris );

	gtk_drag_finish ( ct, TRUE, FALSE, time );
}

static GtkWidget * scan_create ( Dvb5Win *win )
{
	GtkGrid *grid = (GtkGrid *)gtk_grid_new ();
	GtkWidget *wgrid = GTK_WIDGET ( grid );

	gtk_grid_set_row_spacing ( grid, 5 );
	gtk_grid_set_column_spacing ( grid, 10 );
	gtk_grid_set_row_homogeneous    ( grid, FALSE );
	gtk_grid_set_column_homogeneous ( grid, TRUE  );

	set_margin ( 10, wgrid );
	gtk_widget_set_visible ( wgrid, TRUE );

	struct DataDevice { GtkLabel *label_a; GtkWidget *widget_a; GtkLabel *label_b; GtkWidget *widget_b; } data_n[] =
	{
		{ scan_create_label ( "Adapter"    ), scan_create_spin  ( 0, 16, 1, 0, "Adapter", win ), scan_create_label ( "Frontend"     ), scan_create_spin  ( 0, 16, 1, 0, "Frontend", win ) },
		{ scan_create_label ( "Demux"      ), scan_create_spin  ( 0, 16, 1, 0, "Demux",   win ), scan_create_label ( "Timeout"      ), scan_create_spin  ( 1, 50, 1, 2, "Timeout",  win ) },
		{ scan_create_label ( "Freqs-only" ), scan_create_check ( FALSE, "Freqs-only",    win ), scan_create_label ( "Get frontend" ), scan_create_check ( FALSE, "Get frontend",   win ) },
		{ scan_create_label ( "Nit"        ), scan_create_check ( FALSE, "Nit",           win ), scan_create_label ( "Other nit"    ), scan_create_check ( FALSE, "Other nit",      win ) },
		{ scan_create_label ( "LNB"        ), scan_create_combo_lnb ( "LNB",              win ), scan_create_label ( "DISEqC"       ), scan_create_spin  ( -1, 50, 1, -1, "DISEqC",      win ) },
		{ scan_create_label ( "LNA"        ), scan_create_combo_lna ( "LNA",              win ), scan_create_label ( "Wait DISEqC"  ), scan_create_spin  (  0, 50, 1,  0, "Wait DISEqC", win ) }
	};

	uint8_t d = 0; for ( d = 0; d < G_N_ELEMENTS ( data_n ); d++ )
	{
		gtk_grid_attach ( grid, GTK_WIDGET ( data_n[d].label_a  ), 0, d, 1, 1 );
		gtk_grid_attach ( grid, GTK_WIDGET ( data_n[d].widget_a ), 1, d, 1, 1 );
		gtk_grid_attach ( grid, GTK_WIDGET ( data_n[d].label_b  ), 2, d, 1, 1 );
		gtk_grid_attach ( grid, GTK_WIDGET ( data_n[d].widget_b ), 3, d, 1, 1 );
	}

	gtk_grid_attach ( grid, GTK_WIDGET ( scan_create_in_out_entry ( "Initial file",     "Open", win ) ), 0, d, 2, 1 );
	gtk_grid_attach ( grid, GTK_WIDGET ( scan_create_in_out_entry ( "dvb_channel.conf", "Save", win ) ), 2, d, 2, 1 );

	gtk_drag_dest_set ( wgrid, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY );
	gtk_drag_dest_add_uri_targets  ( wgrid );

	g_signal_connect ( wgrid, "drag-data-received", G_CALLBACK ( scan_signal_drag_in ), win );

	return wgrid;
}

// ***** Zap *****

static void zap_treeview_append ( const char *channel, uint16_t sid, uint16_t apid, uint16_t vpid, Dvb5Win *win )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( win->treeview );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );
	if ( ind >= UINT16_MAX ) return;

	gtk_list_store_append ( GTK_LIST_STORE ( model ), &iter );
	gtk_list_store_set    ( GTK_LIST_STORE ( model ), &iter, COL_NUM, ind + 1, COL_SID, sid, COL_VPID, vpid, COL_APID, apid, COL_CHL, channel, -1 );
}

static void zap_signal_trw_act ( GtkTreeView *tree_view, GtkTreePath *path, G_GNUC_UNUSED GtkTreeViewColumn *column, Dvb5Win *win )
{
	uint8_t num_dmx = (uint8_t)gtk_combo_box_get_active ( GTK_COMBO_BOX ( win->combo_dmx ) );

	uint8_t descr_num = out_demux_n[num_dmx].descr_num; // enum dmx_output

	const char *file = gtk_entry_get_text ( win->entry_file );

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( tree_view );

	if ( !gtk_tree_model_get_iter ( model, &iter, path ) ) return;

	g_autofree char *channel = NULL;
	gtk_tree_model_get ( model, &iter, COL_CHL, &channel, -1 );

	g_signal_emit_by_name ( win->dvb, "dvb-zap", win->adapter, win->frontend, win->demux, descr_num, channel, file );
}

static gboolean zap_signal_parse_dvb_file ( const char *file, Dvb5Win *win )
{
	if ( file == NULL ) return FALSE;
	if ( !g_file_test ( file, G_FILE_TEST_EXISTS ) ) return FALSE;

	struct dvb_file *dvb_file;
	struct dvb_entry *entry;

	dvb_file = dvb_read_file_format ( file, 0, FILE_DVBV5 );

	if ( !dvb_file )
	{
		g_warning ( "%s:: Read file format ( only DVBV5 ) failed.", __func__ );

		return FALSE;
	}

	gtk_list_store_clear ( GTK_LIST_STORE ( gtk_tree_view_get_model ( win->treeview ) ) );

	for ( entry = dvb_file->first_entry; entry != NULL; entry = entry->next )
	{
		if ( entry->channel  ) zap_treeview_append ( entry->channel,  entry->service_id, ( entry->audio_pid ) ? entry->audio_pid[0] : 0, ( entry->video_pid ) ? entry->video_pid[0] : 0, win );
		if ( entry->vchannel ) zap_treeview_append ( entry->vchannel, entry->service_id, ( entry->audio_pid ) ? entry->audio_pid[0] : 0, ( entry->video_pid ) ? entry->video_pid[0] : 0, win );
	}

	dvb_file_free ( dvb_file );

	gtk_entry_set_text ( win->entry_file, file );

	return TRUE;
}

static void zap_signal_drag_in ( G_GNUC_UNUSED GtkTreeView *tree_view, GdkDragContext *ct, G_GNUC_UNUSED int x, G_GNUC_UNUSED int y, GtkSelectionData *s_data, G_GNUC_UNUSED uint info, guint32 time, Dvb5Win *win )
{
	char **uris = gtk_selection_data_get_uris ( s_data );

	g_autofree char *file = g_filename_from_uri ( uris[0], NULL, NULL );

	zap_signal_parse_dvb_file ( file, win );

	g_strfreev ( uris );

	gtk_drag_finish ( ct, TRUE, FALSE, time );
}

static void zap_treeview_stop_dmx_rec_all_toggled ( Dvb5Win *win )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( win->treeview );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );
	if ( ind == 0 ) return;

	gboolean valid, active = FALSE;
	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid; valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		gtk_tree_model_get ( model, &iter, COL_PRW, &active, -1 );

		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_SIZE,   "", -1 );
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_REC, FALSE, -1 );
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PRW, FALSE, -1 );
	}
}

static char * zap_time_to_str ( void )
{
	GDateTime *date = g_date_time_new_now_local ();

	char *str_time = g_date_time_format ( date, "%j-%Y-%H%M%S" );

	g_date_time_unref ( date );

	return str_time;
}

static gboolean zap_check_dvr ( const char *dvr )
{
	int dvr_fd = open ( dvr, O_RDONLY );

	if ( dvr_fd == -1 ) return FALSE;

	close ( dvr_fd );

	return TRUE;
}

static gboolean zap_check_player ( const char *player )
{
	if ( player )
	{
		char player_path[PATH_MAX];
		sprintf ( player_path, "/usr/bin/%s", player );

		if ( !g_file_test ( player_path, G_FILE_TEST_EXISTS ) ) return FALSE;
	}

	return TRUE;
}

static char * zap_rec_get_path ( const char *dir, const char *name, Dvb5Win *win )
{
	g_autofree char *date = zap_time_to_str ();

	char name_new[PATH_MAX];
	sprintf ( name_new, "%s-%s.ts", name, date );

	char *file = file_save ( dir, name_new, GTK_WINDOW ( win ) );

	return file;
}

static gboolean zap_monitor ( Monitor *monitor )
{
	if ( !GTK_IS_TREE_VIEW ( monitor->treeview ) ) { monitor->active = 0; return FALSE; }

	char str_path[10];
	sprintf ( str_path, "%u", monitor->path );

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( monitor->treeview );
	GtkTreePath  *path  = gtk_tree_path_new_from_string ( str_path );

	gtk_tree_model_get_iter ( model, &iter, path );
	gtk_tree_path_free ( path );

	gboolean active;
	gtk_tree_model_get ( model, &iter, monitor->column, &active, -1 );

	if ( !active ) { monitor->active = 0; gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_SIZE, " ", -1 ); return FALSE; }

	if ( active )
	{
		g_autofree char *str_size = g_format_size ( monitor->size_file );
		g_autofree char *str = g_strdup_printf ( "%u Kbps / %s", monitor->bitrate, str_size );

		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_SIZE, str, -1 );
	}

	return TRUE;
}

static Monitor * zap_create_monitor ( enum col_tree col_num, char *path_str, Dvb5Win *win )
{
		Monitor *monitor = g_new0 ( Monitor, 1 );
		monitor->column = col_num;
		monitor->active = 1;
		monitor->bitrate = 0;
		monitor->size_file = 0;
		monitor->treeview = win->treeview;
		monitor->path = (uint16_t)atoi ( path_str );

	return monitor;
}

static void zap_rec_toggled ( G_GNUC_UNUSED GtkCellRendererToggle *toggle, char *path_str, Dvb5Win *win )
{
	// if ( !win->fe_lock ) { dvb5_message_dialog ( "", "Zap?", GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) ); return; }

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( win->treeview );

	GtkTreePath *path = gtk_tree_path_new_from_string ( path_str );
	gtk_tree_model_get_iter ( model, &iter, path );

	gboolean toggle_item;
	gtk_tree_model_get ( model, &iter, COL_REC, &toggle_item, -1 );

	toggle_item = !toggle_item;
	gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_SIZE, "", -1 );
	gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_REC, toggle_item, -1 );

	g_debug ( "%s: toggle_item %d | path_str %d ",  __func__, toggle_item, atoi ( path_str ) );

	if ( toggle_item )
	{
		g_autofree char *channel = NULL;
		gtk_tree_model_get ( model, &iter, COL_CHL, &channel, -1 );

		uint sid = 0, vpid = 0, apid = 0;
		gtk_tree_model_get ( model, &iter, COL_SID,  &sid,  -1 );
		gtk_tree_model_get ( model, &iter, COL_VPID, &vpid, -1 );
		gtk_tree_model_get ( model, &iter, COL_APID, &apid, -1 );

		uint16_t pids[] = { (uint16_t)sid, (uint16_t)vpid, (uint16_t)apid };

		const char *dir = gtk_entry_get_text ( win->entry_rec );
		g_autofree char *file_rec = zap_rec_get_path ( dir, channel, win );

		if ( !file_rec )
		{
			gtk_tree_path_free ( path );
			gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_REC, FALSE, -1 );

			return;
		}

		Monitor *monitor = zap_create_monitor ( COL_REC, path_str, win );

		const char *res = dmx_rec_create ( win->adapter, win->demux, file_rec, 3, pids, monitor );

		if ( res )
		{
			free ( monitor );
			dvb5_message_dialog ( "", res, GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) );
			gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_REC, FALSE, -1 );
		}
		else
			g_timeout_add_seconds ( 1, (GSourceFunc)zap_monitor, monitor );
	}

	gtk_tree_path_free ( path );
}

static void zap_prw_toggled ( G_GNUC_UNUSED GtkCellRendererToggle *toggle, char *path_str, Dvb5Win *win )
{
	// if ( !win->fe_lock ) { dvb5_message_dialog ( "", "Zap?", GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) ); return; }

	const char *player = gtk_entry_get_text ( win->entry_play );

	if ( !zap_check_player ( player ) ) { dvb5_message_dialog ( player, g_strerror ( errno ), GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) ); return; }

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( win->treeview );

	GtkTreePath *path = gtk_tree_path_new_from_string ( path_str );
	gtk_tree_model_get_iter ( model, &iter, path );

	gboolean toggle_item;
	gtk_tree_model_get ( model, &iter, COL_PRW, &toggle_item, -1 );

	toggle_item = !toggle_item;
	gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_SIZE, "", -1 );
	gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PRW, toggle_item, -1 );

	g_debug ( "%s: toggle_item %d | path_str %s ",  __func__, toggle_item, path_str );

	if ( toggle_item )
	{
		g_autofree char *channel = NULL;
		gtk_tree_model_get ( model, &iter, COL_CHL, &channel, -1 );

		uint sid = 0, vpid = 0, apid = 0;
		gtk_tree_model_get ( model, &iter, COL_SID,  &sid,  -1 );
		gtk_tree_model_get ( model, &iter, COL_VPID, &vpid, -1 );
		gtk_tree_model_get ( model, &iter, COL_APID, &apid, -1 );

		uint16_t pids[] = { (uint16_t)sid, (uint16_t)vpid, (uint16_t)apid };

		g_autofree char *date = zap_time_to_str ();

		char file_new[PATH_MAX];
		sprintf ( file_new, "/tmp/%s", date );

		Monitor *monitor = zap_create_monitor ( COL_PRW, path_str, win );

		const char *res = dmx_prw_create ( win->adapter, win->demux, file_new, 3, pids, monitor, player );

		if ( res )
		{
			free ( monitor );
			dvb5_message_dialog ( "", res, GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) );
			gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PRW, FALSE, -1 );
		}
		else
			g_timeout_add_seconds ( 1, (GSourceFunc)zap_monitor, monitor );
	}

	gtk_tree_path_free ( path );
}

static GtkScrolledWindow * zap_create_tree ( Dvb5Win *win )
{
	GtkScrolledWindow *scroll = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_visible ( GTK_WIDGET ( scroll ), TRUE );

	GtkListStore *store = gtk_list_store_new ( NUM_COLS, G_TYPE_UINT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING );

	win->treeview = (GtkTreeView *)gtk_tree_view_new_with_model ( GTK_TREE_MODEL ( store ) );
	gtk_widget_set_visible ( GTK_WIDGET ( win->treeview ), TRUE );

	gtk_drag_dest_set ( GTK_WIDGET ( win->treeview ), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY );
	gtk_drag_dest_add_uri_targets  ( GTK_WIDGET ( win->treeview ) );

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	struct Column { const char *name; const char *type; uint8_t num; } column_n[] =
	{
		{ "Num",        	"text",   COL_NUM  },
		{ "Rec",        	"active", COL_REC  },
		{ "Prw",        	"active", COL_PRW  },
		{ "Channel",    	"text",   COL_CHL  },
		{ "Bitrate / Size", "text",   COL_SIZE },
		{ "SID",      		"text",   COL_SID  },
		{ "Video",      	"text",   COL_VPID },
		{ "Audio",      	"text",   COL_APID }
	};

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( column_n ); c++ )
	{
		if ( c == COL_REC || c == COL_PRW )
		{
			renderer = gtk_cell_renderer_toggle_new ();

			if ( c == COL_REC ) g_signal_connect ( renderer, "toggled", G_CALLBACK ( zap_rec_toggled ), win );
			if ( c == COL_PRW ) g_signal_connect ( renderer, "toggled", G_CALLBACK ( zap_prw_toggled ), win );
		}
		else
			renderer = gtk_cell_renderer_text_new ();

		column = gtk_tree_view_column_new_with_attributes ( column_n[c].name, renderer, column_n[c].type, column_n[c].num, NULL );

		if ( c == COL_SID || c == COL_VPID || c == COL_APID ) gtk_tree_view_column_set_visible ( column, FALSE );

		gtk_tree_view_append_column ( win->treeview, column );
	}

	gtk_container_add ( GTK_CONTAINER ( scroll ), GTK_WIDGET ( win->treeview ) );
	g_object_unref ( G_OBJECT (store) );

	g_signal_connect ( win->treeview, "row-activated",      G_CALLBACK ( zap_signal_trw_act ), win );
	g_signal_connect ( win->treeview, "drag-data-received", G_CALLBACK ( zap_signal_drag_in ), win );

	return scroll;
}

static void zap_dvr_play_stop ( void )
{
	int pid = 0, SIZE = 1024;

	char line[SIZE];

	FILE *fp = popen ( "ps -T -f -o pid:1,args", "r" );

	while ( !feof (fp) )
	{
		if ( fgets ( line, SIZE, fp ) )
		{
			if ( g_strrstr ( line, "dvr" ) ) pid = (int)strtoul ( line, NULL, 10 );
		}
	}

	pclose ( fp );

	if ( pid ) kill ( pid, SIGINT );
}

static gboolean zap_monitor_dvr_rec ( Dvb5Win *win )
{
	if ( !GTK_IS_TREE_VIEW ( win->treeview ) ) return FALSE;

	if ( win->stop_dvr_rec ) { gtk_label_set_text ( win->dvr_rec, "" ); win->monitor_dvr->active = 0; return FALSE; }

	g_autofree char *str_size = g_format_size ( win->monitor_dvr->size_file );
	g_autofree char *str = g_strdup_printf ( "%u Kbps / %s", win->monitor_dvr->bitrate, str_size );

	gtk_label_set_text ( win->dvr_rec, str );

	return TRUE;
}

static void zap_dvr_play ( G_GNUC_UNUSED GtkButton *button, Dvb5Win *win )
{
	if ( !win->fe_lock ) { dvb5_message_dialog ( "", "Zap?", GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) ); return; }

	char dvrdev[PATH_MAX];
	sprintf ( dvrdev, "/dev/dvb/adapter%d/dvr0", win->adapter );

	if ( !zap_check_dvr ( dvrdev ) ) { dvb5_message_dialog ( dvrdev, g_strerror ( errno ), GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) ); return; }

	const char *player = gtk_entry_get_text ( win->entry_play );

	if ( !zap_check_player ( player ) ) { dvb5_message_dialog ( player, g_strerror ( errno ), GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) ); return; }

	char dvr_play[PATH_MAX];
	sprintf ( dvr_play, "%s /dev/dvb/adapter%d/dvr0", player, win->adapter );

	GAppInfo *app = g_app_info_create_from_commandline ( dvr_play, NULL, G_APP_INFO_CREATE_NONE, NULL );

	GError *error = NULL;
	g_app_info_launch ( app, NULL, NULL, &error );

	if ( error ) { dvb5_message_dialog ( "", error->message, GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) ); g_warning ( "%s: %s", __func__, error->message ); g_error_free ( error ); }
}

static void zap_dvr_rec ( G_GNUC_UNUSED GtkButton *button, Dvb5Win *win )
{
	if ( !win->fe_lock ) { dvb5_message_dialog ( "", "Zap?", GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) ); return; }

	char dvrdev[PATH_MAX];
	sprintf ( dvrdev, "/dev/dvb/adapter%d/dvr0", win->adapter );

	if ( !zap_check_dvr ( dvrdev ) ) { dvb5_message_dialog ( dvrdev, g_strerror ( errno ), GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) ); return; }

	g_autofree char *channel = NULL;

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( win->treeview );

	if ( gtk_tree_selection_get_selected ( gtk_tree_view_get_selection ( win->treeview ), NULL, &iter ) )
	{
		gtk_tree_model_get ( model, &iter, COL_CHL, &channel, -1 );
	}

	const char *dir = gtk_entry_get_text ( win->entry_rec );
	g_autofree char *file_rec = zap_rec_get_path ( dir, ( channel ) ? channel : "Record", win );

	if ( !file_rec ) return;

	Monitor *monitor = zap_create_monitor ( COL_NUM, "0", win );

	win->stop_dvr_rec = FALSE;
	win->monitor_dvr = monitor;

	const char *res = dvr_rec_create ( dvrdev, file_rec, monitor );

	if ( res )
	{
		free ( monitor );
		win->monitor_dvr = NULL;
		dvb5_message_dialog ( "", res, GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) );
	}
	else
		g_timeout_add_seconds ( 1, (GSourceFunc)zap_monitor_dvr_rec, win );
}

static void zap_popover_hide ( G_GNUC_UNUSED GtkButton *button, GtkPopover *popover )
{
	gtk_widget_set_visible ( GTK_WIDGET ( popover ), FALSE );
}

static GtkButton * zap_create_button ( const char *text, GtkPopover *popover, fp func, Dvb5Win *win )
{
	GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( text, GTK_ICON_SIZE_MENU );

	if ( popover ) g_signal_connect ( button, "clicked", G_CALLBACK ( zap_popover_hide ), popover );

	g_signal_connect ( button, "clicked", G_CALLBACK ( func ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );

	return button;
}

static GtkBox * zap_create_box_popover ( GtkPopover *popover, Dvb5Win *win )
{
	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_set_spacing ( v_box, 5 );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	GtkLabel *label_play = scan_create_label ( "Dvr: Play"   );
	GtkLabel *label_rec  = scan_create_label ( "Dvr: Record" );

	GtkButton *button_play = zap_create_button ( "dvb-start", popover, zap_dvr_play, win );
	GtkButton *button_rec  = zap_create_button ( "dvb-rec",   popover, zap_dvr_rec,  win );

	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	gtk_box_pack_start ( h_box, GTK_WIDGET ( button_play ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( label_play  ), TRUE,  TRUE,  0 );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	gtk_box_pack_start ( h_box, GTK_WIDGET ( button_rec ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( label_rec  ), TRUE,  TRUE,  0 );

	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	return v_box;
}

static void zap_signal_entry_play ( G_GNUC_UNUSED GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, GtkPopover *popover )
{
	if ( icon_pos == GTK_ENTRY_ICON_PRIMARY )
	{			
		cairo_rectangle_int_t rect;

		gtk_entry_get_icon_area ( entry, icon_pos, &rect );
		gtk_popover_set_pointing_to ( popover, &rect );
		gtk_popover_popup ( popover );

		g_object_set_data ( G_OBJECT (entry), "popover-icon-pos", GUINT_TO_POINTER (icon_pos) );
	}
}

static GtkEntry * zap_create_entry ( const char *text, const char *info, gboolean popover, Dvb5Win *win )
{
	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( entry, text );
	g_object_set ( entry, "editable", ( info ) ? FALSE : TRUE, NULL );

	if ( info )
	{
		gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_PRIMARY,   "dvb-info" );
		gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_SECONDARY, "folder"   );
		gtk_entry_set_icon_tooltip_text ( GTK_ENTRY ( entry ), GTK_ENTRY_ICON_PRIMARY, info );
	}

	if ( popover )
	{
		gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_PRIMARY, "dvb-list" );

		GtkPopover *popover = (GtkPopover *)gtk_popover_new ( GTK_WIDGET ( entry ) );
		gtk_popover_set_position ( popover, GTK_POS_TOP );
		gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( zap_create_box_popover ( popover, win ) ) );
		gtk_container_set_border_width ( GTK_CONTAINER ( popover ), 2 );

		g_signal_connect ( entry, "icon-press", G_CALLBACK ( zap_signal_entry_play ), popover );
	}

	gtk_widget_set_visible ( GTK_WIDGET ( entry ), TRUE );

	return entry;
}

static void zap_combo_dmx_add ( uint8_t n_elm, Dvb5Win *win )
{
	uint8_t c = 0; for ( c = 0; c < n_elm; c++ )
		gtk_combo_box_text_append_text ( win->combo_dmx, out_demux_n[c].name );

	gtk_combo_box_set_active ( GTK_COMBO_BOX ( win->combo_dmx ), 2 );
}

static void zap_signal_entry_rec ( GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, Dvb5Win *win )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{			
		const char *dir  = gtk_entry_get_text  ( entry );
		g_autofree char *file = file_open ( dir, TRUE, GTK_WINDOW ( win ) );

		if ( file ) gtk_entry_set_text ( entry, file );
	}
}

static void zap_signal_entry_file ( G_GNUC_UNUSED GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, Dvb5Win *win )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{			
		g_autofree char *file = file_open ( g_get_home_dir (), FALSE, GTK_WINDOW ( win ) );

		if ( file ) zap_signal_parse_dvb_file ( file, win );
	}
}

static void zap_treeview_clear ( G_GNUC_UNUSED GtkButton *button, Dvb5Win *win )
{
	gtk_entry_set_text ( win->entry_file, "" );
	gtk_list_store_clear ( GTK_LIST_STORE ( gtk_tree_view_get_model ( win->treeview ) ) );
}

static GtkWidget * zap_grid ( Dvb5Win *win )
{
	GtkGrid *grid = (GtkGrid *)gtk_grid_new ();
	GtkWidget *wgrid = GTK_WIDGET ( grid );

	gtk_grid_set_row_spacing ( grid, 5 );
	gtk_grid_set_column_spacing ( grid, 5 );
	gtk_grid_set_row_homogeneous    ( grid, FALSE );
	gtk_grid_set_column_homogeneous ( grid, TRUE  );

	win->entry_play = zap_create_entry ( "ffplay", NULL, TRUE, win );
	win->entry_rec  = zap_create_entry ( g_get_home_dir (), "Record folder", FALSE, win );
	win->entry_file = zap_create_entry ( "dvb_channel.conf", "Format only DVBV5", FALSE, win );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	win->combo_dmx = (GtkComboBoxText *) gtk_combo_box_text_new ();
	zap_combo_dmx_add ( G_N_ELEMENTS ( out_demux_n ), win );

	GtkButton *button_clear = zap_create_button ( "dvb-clear", NULL, zap_treeview_clear, win );

	gtk_box_pack_start ( h_box, GTK_WIDGET ( win->combo_dmx ), TRUE, TRUE, 0 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( button_clear   ), TRUE, TRUE, 0 );

	gtk_widget_set_visible ( wgrid, TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( win->combo_dmx ), TRUE );

	struct DataDevice { GtkWidget *widget_a; GtkWidget *widget_b; } data_n[] =
	{
		{ GTK_WIDGET ( win->entry_rec  ), GTK_WIDGET ( win->entry_play ) },
		{ GTK_WIDGET ( win->entry_file ), GTK_WIDGET ( h_box           ) }
	};

	uint8_t d = 0; for ( d = 0; d < G_N_ELEMENTS ( data_n ); d++ )
	{
		gtk_grid_attach ( grid, GTK_WIDGET ( data_n[d].widget_a ), 0, d, 1, 1 );
		gtk_grid_attach ( grid, GTK_WIDGET ( data_n[d].widget_b ), 1, d, 1, 1 );
	}

	return wgrid;
}

static GtkWidget * zap_create ( Dvb5Win *win )
{
	GtkBox *vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	GtkWidget *wvbox = GTK_WIDGET ( vbox );

	set_margin ( 10, wvbox );
	gtk_box_set_spacing ( vbox, 10 );
	gtk_widget_set_visible ( wvbox, TRUE );

	gtk_box_pack_start ( vbox, GTK_WIDGET ( zap_create_tree ( win ) ), TRUE,  TRUE,  0 );
	gtk_box_pack_end   ( vbox, GTK_WIDGET ( zap_grid        ( win ) ), FALSE, FALSE, 0 );

	g_signal_connect ( win->entry_rec,  "icon-press", G_CALLBACK ( zap_signal_entry_rec  ), win );
	g_signal_connect ( win->entry_file, "icon-press", G_CALLBACK ( zap_signal_entry_file ), win );

	return wvbox;
}

// ***** Status *****

static void status_clicked_scan ( G_GNUC_UNUSED GtkButton *button, Dvb5Win *win )
{
	const char *file_int = gtk_entry_get_text ( win->entry_int );

	if ( !g_file_test ( file_int, G_FILE_TEST_EXISTS ) ) { dvb5_message_dialog ( file_int, g_strerror ( errno ), GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) ); return; }

	gboolean file_new = FALSE;
	char file_out_new[PATH_MAX];
	const char *f_out = gtk_entry_get_text ( win->entry_out );

	if ( g_str_equal ( f_out, "dvb_channel.conf" ) )
	{
		file_new = TRUE;
		sprintf ( file_out_new, "%s/dvb_channel.conf", g_get_home_dir () );
	}

	const char *file_out = ( file_new ) ? file_out_new : gtk_entry_get_text ( win->entry_out );

	g_autofree char *fm_int = gtk_combo_box_text_get_active_text ( win->combo_int );
	g_autofree char *fm_out = gtk_combo_box_text_get_active_text ( win->combo_out );

	const char *lnb = gtk_combo_box_get_active_id ( GTK_COMBO_BOX ( win->combo_lnb ) );
	g_autofree char *lna = gtk_combo_box_text_get_active_text ( win->combo_lna );

	g_debug ( "%s: %s, %s, %s, %s ", __func__, lnb, lna, fm_int, fm_out );
	g_debug ( "%s: %s, %s ", __func__, file_int, file_out );

	g_signal_emit_by_name ( win->dvb, "dvb-scan", win->adapter, win->frontend, win->demux, win->time_mult, win->new_freqs, win->get_detect, win->get_nit, win->other_nit, 
		win->sat_num, win->diseqc_wait, lnb, lna, file_int, file_out, fm_int, fm_out );
}

static gboolean zap_timeout_stop ( Dvb5Win *win )
{
	if ( !GTK_IS_TREE_VIEW ( win->treeview ) ) return FALSE;

	g_signal_emit_by_name ( win->dvb, "dvb-zap-stop" );

	return FALSE;
}

static void status_clicked_stop ( G_GNUC_UNUSED GtkButton *button, Dvb5Win *win )
{
	zap_dvr_play_stop ();

	win->stop_dvr_rec = TRUE;

	zap_treeview_stop_dmx_rec_all_toggled ( win );

	g_signal_emit_by_name ( win->dvb, "dvb-scan-stop" );

	if ( !win->fe_lock )
		g_signal_emit_by_name ( win->dvb, "dvb-zap-stop" );
	else
		g_timeout_add_seconds ( 2, (GSourceFunc)zap_timeout_stop, win );
}

static void status_clicked_dark ( G_GNUC_UNUSED GtkButton *button, G_GNUC_UNUSED Dvb5Win *win )
{
	gboolean dark = FALSE;
	g_object_get ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", &dark, NULL );
	g_object_set ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", !dark, NULL );
}

static void status_clicked_info ( G_GNUC_UNUSED GtkButton *button, Dvb5Win *win )
{
	dvbv5_about ( win );
}

static void status_clicked_exit ( G_GNUC_UNUSED GtkButton *button, Dvb5Win *win )
{
	gtk_widget_destroy ( GTK_WIDGET ( win ) );
}

static void status_sw_layers ( GObject *gobject, G_GNUC_UNUSED GParamSpec *pspec, Dvb5Win *win )
{
	gboolean state = gtk_switch_get_state ( GTK_SWITCH ( gobject ) );

	uint c = 0; for ( c = 0; c < MAX_STATS; c++ )
	{
		gtk_widget_set_visible ( GTK_WIDGET ( win->org_status[c] ), state );
	}
}

static GtkSwitch * status_create_switch ( Dvb5Win *win )
{
	GtkSwitch *gswitch = (GtkSwitch *)gtk_switch_new ();
	gtk_switch_set_state ( gswitch, FALSE );
	g_signal_connect ( gswitch, "notify::active", G_CALLBACK ( status_sw_layers ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( gswitch ), TRUE );

	return gswitch;
}

static void status_changed_combo_msec ( GtkComboBox *combo_box, Dvb5Win *win )
{
	uint16_t msec = 250;
	uint8_t num = (uint8_t)gtk_combo_box_get_active ( combo_box );

	uint8_t num_set = (uint8_t)( num + 1 );

	msec = (uint16_t)( msec * num_set );

	g_signal_emit_by_name ( win->dvb, "dvb-fe-msec", msec );
}

static GtkComboBoxText * status_create_combo ( Dvb5Win *win )
{
	const char *text[] = { "250 msec", "500 msec", "750 msec", "1 sec" };

	GtkComboBoxText *combo_msec = (GtkComboBoxText *) gtk_combo_box_text_new ();
	scan_append_text_combo_box ( combo_msec, text, G_N_ELEMENTS ( text ), 0 );
	g_signal_connect ( combo_msec, "changed", G_CALLBACK ( status_changed_combo_msec ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( combo_msec ), TRUE );

	return combo_msec;
}

static void status_create_layers ( GtkBox *vbox, Dvb5Win *win )
{
	const char *label[MAX_STATS] = { "Layer A: ", "Layer B: ","Layer C: ", "Layer D: " };

	uint8_t c = 0; for ( c = 0; c < MAX_STATS; c++ )
	{
		win->org_status[c] = scan_create_label ( label[c] );

		gtk_widget_set_visible ( GTK_WIDGET ( win->org_status[c] ), FALSE );
		gtk_box_pack_start ( vbox, GTK_WIDGET ( win->org_status[c] ), FALSE, FALSE, 0 );
	}
}

static GtkWidget * status_create ( Dvb5Win *win )
{
	GtkBox *vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	GtkWidget *wvbox = GTK_WIDGET ( vbox );

	set_margin ( 10, wvbox );
	gtk_widget_set_visible ( wvbox, TRUE );

	win->dvb_name = (GtkLabel *)gtk_label_new ( "Dvb Device" );
	gtk_box_pack_start ( vbox, GTK_WIDGET ( win->dvb_name ), FALSE, FALSE, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( win->dvb_name ), TRUE );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	GtkSwitch *gswitch = status_create_switch ( win );
	GtkComboBoxText *combo_msec = status_create_combo ( win );

	gtk_box_pack_end ( h_box, GTK_WIDGET ( combo_msec ), FALSE, FALSE, 0 );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( gswitch    ), FALSE, FALSE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );
	gtk_box_pack_start ( vbox, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	status_create_layers ( vbox, win );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	const char *labels[] = { "dvb-start", "dvb-stop", "dvb-dark", "dvb-info", "dvb-quit" };
	fp funcs[] = { status_clicked_scan, status_clicked_stop, status_clicked_dark, status_clicked_info, status_clicked_exit };

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( labels ); c++ )
	{
		GtkButton *button = zap_create_button ( labels[c], NULL, funcs[c], win );
		gtk_box_pack_start ( h_box, GTK_WIDGET ( button  ), TRUE, TRUE, 0 );
	}

	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );
	gtk_box_pack_end ( vbox, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	win->level = level_new ();
	gtk_box_pack_end ( vbox, GTK_WIDGET ( win->level ), FALSE, FALSE, 5 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 50 );

	win->dvr_rec   = (GtkLabel *)gtk_label_new ( "" );
	win->freq_scan = (GtkLabel *)gtk_label_new ( "" );

	gtk_widget_set_halign ( GTK_WIDGET ( win->dvr_rec   ), GTK_ALIGN_END   );
	gtk_widget_set_halign ( GTK_WIDGET ( win->freq_scan ), GTK_ALIGN_START );

	gtk_box_pack_end   ( h_box, GTK_WIDGET ( win->dvr_rec   ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( win->freq_scan ), FALSE, FALSE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( win->dvr_rec   ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( win->freq_scan ), TRUE );

	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );
	gtk_box_pack_end ( vbox, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	return wvbox;
}

// ***** Base *****

static void dvb5_handler_scan_done ( G_GNUC_UNUSED Dvb *dvb, Dvb5Win *win )
{
	gboolean file_new = FALSE;
	char file_out_new[PATH_MAX];
	const char *f_out = gtk_entry_get_text ( win->entry_out );

	if ( g_str_equal ( f_out, "dvb_channel.conf" ) )
	{
		file_new = TRUE;
		sprintf ( file_out_new, "%s/dvb_channel.conf", g_get_home_dir () );
	}

	zap_signal_parse_dvb_file ( ( file_new ) ? file_out_new : f_out, win );

	g_debug ( "%s: %s ", __func__, ( file_new ) ? file_out_new : f_out );
}

static void dvb5_handler_scan_info ( G_GNUC_UNUSED Dvb *dvb, const char *ret_str, Dvb5Win *win )
{
	dvb5_message_dialog ( "", ret_str, GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) );
}

static void dvb5_handler_dvb_name ( G_GNUC_UNUSED Dvb *dvb, const char *dvb_name, Dvb5Win *win )
{
	gtk_label_set_text ( win->dvb_name, dvb_name );
}

static void dvb5_handler_stats_upd ( G_GNUC_UNUSED Dvb *dvb, uint32_t freq, uint8_t qual, char *sgl, char *snr, uint8_t sgl_p, uint8_t snr_p, gboolean fe_lock, Dvb5Win *win )
{
	win->fe_lock = fe_lock;

	char text[256];
	sprintf ( text, "Freq:  %d ", freq );

	gtk_label_set_text ( win->freq_scan, ( freq ) ? text : "" );

	g_signal_emit_by_name ( win->level, "level-update", qual, sgl, snr, sgl_p, snr_p, fe_lock );
}

static void dvb5_handler_stats_org ( G_GNUC_UNUSED Dvb *dvb, int num, char *text, Dvb5Win *win )
{
	const char *label[MAX_STATS] = { "Layer A: ", "Layer B: ","Layer C: ", "Layer D: " };

	char set_text[256];
	sprintf ( set_text, "%s  %s ", label[num], ( text ) ? text : "" );

	gtk_label_set_text ( win->org_status[num], set_text );
}

static void dvb5_win_destroy ( G_GNUC_UNUSED GtkWindow *window, Dvb5Win *win )
{
	status_clicked_stop ( NULL, win );
}

static void dvb5_win_create ( Dvb5Win *win )
{
	setlocale ( LC_NUMERIC, "C" );

	GtkWindow *window = GTK_WINDOW ( win );
	g_signal_connect ( window, "destroy", G_CALLBACK ( dvb5_win_destroy ), win );

	gtk_window_set_title ( window, "Dvbv5-Gtk");
	gtk_window_set_icon_name ( window, "dvbv5-gtk" );
	gtk_window_set_default_size ( window, 300, 200 );

	GtkBox *main_vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_set_spacing ( main_vbox, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( main_vbox ), TRUE );

	win->notebook = (GtkNotebook *)gtk_notebook_new ();
	gtk_notebook_set_scrollable ( win->notebook, TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( win->notebook ), TRUE );

	const char *pages[]  = { "Scan", "Zap", "Status" };
	GtkWidget  *wpages[] = { scan_create ( win ), zap_create ( win ), status_create ( win ) };

	uint8_t j = 0; for ( j = 0; j < G_N_ELEMENTS ( pages ); j++ )
	{
		gtk_notebook_append_page ( win->notebook, wpages[j], gtk_label_new ( pages[j] ) );
	}

	gtk_notebook_set_tab_pos ( win->notebook, GTK_POS_TOP );
	gtk_box_pack_start ( main_vbox, GTK_WIDGET (win->notebook), TRUE, TRUE, 0 );

	gtk_container_add  ( GTK_CONTAINER ( window ), GTK_WIDGET ( main_vbox ) );
}

static void dvb5_win_open_file ( GFile **files, G_GNUC_UNUSED int n_files, Dvb5Win *win )
{
	g_autofree char *path = g_file_get_path ( files[0] );

	if ( g_str_has_suffix ( path, "channel.conf" ) )
	{
		zap_signal_parse_dvb_file ( path, win );
		gtk_notebook_next_page ( win->notebook );
	}
	else
	{
		gtk_entry_set_text ( win->entry_int, path );
	}
}

static void dvb5_win_init ( Dvb5Win *win )
{
	gtk_icon_theme_add_resource_path ( gtk_icon_theme_get_default (), "/dvbv5" );

	win->adapter   = 0;
	win->frontend  = 0;
	win->demux     = 0;
	win->time_mult = 2;

	win->new_freqs  = 0;
	win->get_detect = 0;
	win->get_nit    = 0;
	win->other_nit  = 0;

	// win->lna = -1;
	// win->lnb = -1;
	win->sat_num = -1;
	win->diseqc_wait = 0;

	win->monitor_dvr = NULL;
	win->stop_dvr_rec = FALSE;

	win->dvb = dvb_new ();

	g_signal_connect ( win->dvb, "dvb-name",      G_CALLBACK ( dvb5_handler_dvb_name  ), win );
	g_signal_connect ( win->dvb, "dvb-scan-info", G_CALLBACK ( dvb5_handler_scan_info ), win );
	g_signal_connect ( win->dvb, "dvb-scan-done", G_CALLBACK ( dvb5_handler_scan_done ), win );

	g_signal_connect ( win->dvb, "stats-org",     G_CALLBACK ( dvb5_handler_stats_org ), win );
	g_signal_connect ( win->dvb, "stats-update",  G_CALLBACK ( dvb5_handler_stats_upd ), win );

	dvb5_win_create ( win );

	g_signal_emit_by_name ( win->dvb, "dvb-info", win->adapter, win->frontend );
}

static void dvb5_win_finalize ( GObject *object )
{
	Dvb5Win *win = DVB5_WIN ( object );

	g_object_unref ( win->dvb );

	G_OBJECT_CLASS ( dvb5_win_parent_class )->finalize ( object );
}

static void dvb5_win_class_init ( Dvb5WinClass *class )
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->finalize = dvb5_win_finalize;
}

Dvb5Win * dvb5_win_new ( GFile **files, int n_files, Dvb5App *app )
{
	Dvb5Win *win = g_object_new ( DVB5_TYPE_WIN, "application", app, NULL );

	if ( n_files ) dvb5_win_open_file ( files, n_files, win );

	return win;
}
