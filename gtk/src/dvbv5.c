/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "dvbv5.h"
#include "file.h"
#include "pref.h"
#include "zap-signals.h"
#include "scan-signals.h"

#include <locale.h>

enum out_dmx
{
	DMX_OUT_ALL_PIDS = 4,
	DMX_OUT_OFF = 5
};

const OutDemux out_demux_n[] =
{
	{ DMX_OUT_DECODER, 	"DMX_OUT_DECODER" 	},
	{ DMX_OUT_TAP, 		"DMX_OUT_TAP"		},
	{ DMX_OUT_TS_TAP, 	"DMX_OUT_TS_TAP" 	},
	{ DMX_OUT_TSDEMUX_TAP, 	"DMX_OUT_TSDEMUX_TAP" 	},
	{ DMX_OUT_ALL_PIDS, 	"DMX_OUT_ALL_PIDS" 	},
	{ DMX_OUT_OFF, 		"DMX_OUT_OFF"		}
};

G_DEFINE_TYPE ( Dvbv5, dvbv5, GTK_TYPE_APPLICATION )

typedef void ( *fp ) ( Dvbv5 *dvbv5 );

static uint8_t verbose = 0;

void dvbv5_message_dialog ( const char *error, const char *file_or_info, GtkMessageType mesg_type, GtkWindow *window )
{
	GtkMessageDialog *dialog = ( GtkMessageDialog *)gtk_message_dialog_new (
		window, GTK_DIALOG_MODAL, mesg_type, GTK_BUTTONS_CLOSE, "%s\n%s", error, file_or_info );

	gtk_dialog_run     ( GTK_DIALOG ( dialog ) );
	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
}

static void dvbv5_about ( Dvbv5 *dvbv5 )
{
	GtkAboutDialog *dialog = (GtkAboutDialog *)gtk_about_dialog_new ();
	gtk_window_set_transient_for ( GTK_WINDOW ( dialog ), dvbv5->window );

	GdkPixbuf *pixbuf = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default (), "dvb-logo", 48, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );

	if ( pixbuf )
	{
		gtk_window_set_icon ( GTK_WINDOW ( dialog ), pixbuf );
		gtk_about_dialog_set_logo ( dialog, pixbuf );
		g_object_unref ( pixbuf );
	}
	else
	{
		gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "display" );
		gtk_about_dialog_set_logo_icon_name ( dialog, "display" );
	}

	const char *authors[] = { "Stepan Perun", " ", NULL };
	const char *issues[]  = { "Mauro Carvalho Chehab", "zampedro", "Ro-Den", " ", NULL };

	gtk_about_dialog_add_credit_section ( dialog, "Issues ( github.com )", issues );

	gtk_about_dialog_set_program_name ( dialog, PROGNAME );
	gtk_about_dialog_set_version ( dialog, VERSION );
	gtk_about_dialog_set_license_type ( dialog, GTK_LICENSE_GPL_2_0 );
	gtk_about_dialog_set_authors ( dialog, authors );
	gtk_about_dialog_set_website ( dialog,   "https://github.com/vl-nix/dvbv5-gtk" );
	gtk_about_dialog_set_copyright ( dialog, "Copyright 2020 Dvbv5-Gtk" );
	gtk_about_dialog_set_comments  ( dialog, "Gtk+3 interface to DVBv5 tool" );

	gtk_dialog_run ( GTK_DIALOG (dialog) );
	gtk_widget_destroy ( GTK_WIDGET (dialog) );
}

static uint8_t _get_delsys ( struct dvb_v5_fe_parms *parms )
{
	uint8_t sys = SYS_UNDEFINED;

	switch ( parms->current_sys )
	{
		case SYS_DVBT:
		case SYS_DVBS:
		case SYS_DVBC_ANNEX_A:
		case SYS_ATSC:
			sys = parms->current_sys;
			break;
		case SYS_DVBC_ANNEX_C:
			sys = SYS_DVBC_ANNEX_A;
			break;
		case SYS_DVBC_ANNEX_B:
			sys = SYS_ATSC;
			break;
		case SYS_ISDBT:
		case SYS_DTMB:
			sys = SYS_DVBT;
			break;
		default:
			sys = SYS_UNDEFINED;
			break;
	}

	return sys;
}

static int _check_frontend ( G_GNUC_UNUSED void *__args, G_GNUC_UNUSED struct dvb_v5_fe_parms *parms )
{
	return 0;
}

static gpointer dvbv5_dvb_scan_thread_run ( Dvbv5 *dvbv5 )
{
	struct dvb_device *dvb = dvbv5->dvb_scan;
	struct dvb_v5_fe_parms *parms = dvb->fe_parms;
	struct dvb_file *dvb_file = NULL, *dvb_file_new = NULL;
	struct dvb_entry *entry;
	struct dvb_open_descriptor *dmx_fd;

	g_mutex_init ( &dvbv5->mutex );

	int count = 0, shift;
	uint32_t freq = 0, sys = _get_delsys ( parms );
	enum dvb_sat_polarization pol;

	dvb_file = dvb_read_file_format ( dvbv5->input_file, sys, dvbv5->input_format );

	if ( !dvb_file )
	{
		g_mutex_lock ( &dvbv5->mutex );
			dvbv5->thread_save = TRUE;
		g_mutex_unlock ( &dvbv5->mutex );

		g_mutex_clear ( &dvbv5->mutex );

		g_critical ( "%s:: Read file format failed.", __func__ );
		return NULL;
	}

	dmx_fd = dvb_dev_open ( dvb, dvbv5->demux_dev, O_RDWR );

	if ( !dmx_fd )
	{
		g_mutex_lock ( &dvbv5->mutex );
			dvbv5->thread_save = TRUE;
		g_mutex_unlock ( &dvbv5->mutex );

		g_mutex_clear ( &dvbv5->mutex );
		dvb_file_free ( dvb_file );

		perror ( "opening demux failed" );
		return NULL;
	}

	for ( entry = dvb_file->first_entry; entry != NULL; entry = entry->next )
	{
		g_mutex_lock ( &dvbv5->mutex );
		if ( dvbv5->window == NULL )
		{
			dvb_file_free ( dvb_file );
			if ( dvb_file_new ) dvb_file_free ( dvb_file_new );
			dvb_dev_close ( dmx_fd );

			dvbv5->thread_save = TRUE;
			g_mutex_unlock ( &dvbv5->mutex );
			g_mutex_clear  ( &dvbv5->mutex );
			return NULL;
		}
		g_mutex_unlock ( &dvbv5->mutex );

		struct dvb_v5_descriptors *dvb_scan_handler = NULL;
		uint32_t stream_id;

		if ( dvb_retrieve_entry_prop ( entry, DTV_FREQUENCY, &freq ) ) continue;

		shift = dvb_estimate_freq_shift ( parms );

		if ( dvb_retrieve_entry_prop ( entry, DTV_POLARIZATION, &pol ) ) pol = POLARIZATION_OFF;
		if ( dvb_retrieve_entry_prop ( entry, DTV_STREAM_ID, &stream_id ) ) stream_id = NO_STREAM_ID_FILTER;
		if ( !dvb_new_entry_is_needed ( dvb_file->first_entry, entry, freq, shift, pol, stream_id ) ) continue;

		count++;
		dvb_log ( "Scanning frequency #%d %d", count, freq );

		gboolean sat = FALSE;
		if ( dvb_fe_is_satellite ( parms->current_sys ) ) sat = TRUE;

		g_mutex_lock ( &dvbv5->mutex );
			dvbv5->freq_scan = ( sat ) ? freq / 1000 : freq / 1000000;
		g_mutex_unlock ( &dvbv5->mutex );

		dvb_scan_handler = dvb_dev_scan ( dmx_fd, entry, &_check_frontend, NULL, dvbv5->scan->other_nit, dvbv5->scan->time_mult );

		g_mutex_lock ( &dvbv5->mutex );
			if ( dvb_scan_handler   ) dvbv5->progs_scan += dvb_scan_handler->num_program;
			if ( dvbv5->thread_stop ) parms->abort = 1;
		g_mutex_unlock ( &dvbv5->mutex );

		if ( parms->abort )
		{
			dvb_scan_free_handler_table ( dvb_scan_handler );
			break;
		}

		if ( !dvb_scan_handler )
			continue;

		dvb_store_channel ( &dvb_file_new, parms, dvb_scan_handler, dvbv5->scan->get_detect, dvbv5->scan->get_nit );

		if ( !dvbv5->scan->new_freqs )
			dvb_add_scaned_transponders ( parms, dvb_scan_handler, dvb_file->first_entry, entry );

		dvb_scan_free_handler_table ( dvb_scan_handler );
	}

	g_mutex_lock ( &dvbv5->mutex );
	if ( dvbv5->window == NULL )
	{
		dvb_file_free ( dvb_file );
		if ( dvb_file_new ) dvb_file_free ( dvb_file_new );
		dvb_dev_close ( dmx_fd );

		dvbv5->thread_save = TRUE;
		g_mutex_unlock ( &dvbv5->mutex );
		g_mutex_clear  ( &dvbv5->mutex );
		return NULL;
	}
	g_mutex_unlock ( &dvbv5->mutex );

	if ( dvb_file_new ) dvb_write_file_format ( dvbv5->output_file, dvb_file_new, parms->current_sys, dvbv5->output_format );

	dvb_file_free ( dvb_file );
	if ( dvb_file_new ) dvb_file_free ( dvb_file_new );

	dvb_dev_close ( dmx_fd );

	g_mutex_lock ( &dvbv5->mutex );
		dvbv5->thread_save = TRUE;
		dvbv5->stat_stop   = TRUE;
		if ( dvbv5->output_format == FILE_DVBV5 ) zap_signal_parse_dvb_file ( dvbv5->output_file, dvbv5 );
	g_mutex_unlock ( &dvbv5->mutex );

	g_mutex_clear ( &dvbv5->mutex );

	return NULL;
}

static void dvbv5_dvb_scan_auto_free ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->debug ) g_message ( "%s: %s ", __func__, ( dvbv5->dvb_scan ) ? "True" : "False" );

	if ( dvbv5->dvb_scan )
	{
		dvb_dev_free ( dvbv5->dvb_scan );
		dvbv5->dvb_scan = NULL;
		dvbv5->demux_dev = NULL;
	}
}

static uint8_t dvbv5_get_val_togglebutton ( GtkCheckButton *checkbutton, gboolean debug )
{
	gboolean set = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON ( checkbutton ) );
	const char *name = gtk_widget_get_name  ( GTK_WIDGET ( checkbutton ) );

	uint8_t ret = ( set ) ? 1 : 0;

	if ( debug ) g_message ( "%s:: %s | set value = %d ", __func__, name, ret );

	return ret;
}

static uint8_t dvbv5_get_val_spinbutton ( GtkSpinButton *spinbutton, gboolean debug )
{
	gtk_spin_button_update ( spinbutton );

	uint8_t val = gtk_spin_button_get_value_as_int ( spinbutton );
	const char *name = gtk_widget_get_name  ( GTK_WIDGET ( spinbutton ) );

	if ( debug ) g_message ( "%s:: %s | set value = %d ", __func__, name, val );

	return val;
}

static gboolean _dvbv5_dvb_scan_init ( Dvbv5 *dvbv5 )
{
	dvbv5_dvb_scan_auto_free ( dvbv5 );
	dvbv5->dvb_scan = dvb_dev_alloc ();

	if ( !dvbv5->dvb_scan ) return FALSE;

	dvbv5->scan->new_freqs  = dvbv5_get_val_togglebutton ( dvbv5->scan->checkbutton[0], dvbv5->debug );
	dvbv5->scan->get_detect = dvbv5_get_val_togglebutton ( dvbv5->scan->checkbutton[1], dvbv5->debug );
	dvbv5->scan->get_nit    = dvbv5_get_val_togglebutton ( dvbv5->scan->checkbutton[2], dvbv5->debug );
	dvbv5->scan->other_nit  = dvbv5_get_val_togglebutton ( dvbv5->scan->checkbutton[3], dvbv5->debug );

	dvbv5->scan->time_mult   = dvbv5_get_val_spinbutton ( dvbv5->scan->spinbutton[3], dvbv5->debug );
	dvbv5->scan->sat_num     = dvbv5_get_val_spinbutton ( dvbv5->scan->spinbutton[4], dvbv5->debug );
	dvbv5->scan->diseqc_wait = dvbv5_get_val_spinbutton ( dvbv5->scan->spinbutton[5], dvbv5->debug );

	dvb_dev_set_log ( dvbv5->dvb_scan, 0, NULL );
	dvb_dev_find ( dvbv5->dvb_scan, NULL, NULL );

	struct dvb_dev_list *dvb_dev = dvb_dev_seek_by_adapter ( dvbv5->dvb_scan, dvbv5->scan->adapter, dvbv5->scan->demux, DVB_DEVICE_DEMUX );

	if ( !dvb_dev )
	{
		g_critical ( "%s:: Couldn't find demux device node.", __func__ );
		dvbv5_message_dialog ( "DVB_DEVICE_DEMUX", "Couldn't find demux device.", GTK_MESSAGE_ERROR, dvbv5->window );

		return FALSE;
	}

	dvbv5->demux_dev = dvb_dev->sysname;
	g_message ( "%s:: scan->demux_dev: %s ", __func__, dvbv5->demux_dev );

	dvb_dev = dvb_dev_seek_by_adapter ( dvbv5->dvb_scan, dvbv5->scan->adapter, dvbv5->scan->frontend, DVB_DEVICE_FRONTEND );

	if ( !dvb_dev ) return FALSE;

	if ( !dvb_dev_open ( dvbv5->dvb_scan, dvb_dev->sysname, O_RDWR ) )
	{
		perror ( "Opening device failed" );
		dvbv5_message_dialog ( "DVB_DEVICE_FRONTEND", "Opening device failed.", GTK_MESSAGE_ERROR, dvbv5->window );

		return FALSE;
	}

	struct dvb_v5_fe_parms *parms = dvbv5->dvb_scan->fe_parms;

	if ( dvbv5->scan->lnb >= 0 ) parms->lnb = dvb_sat_get_lnb ( dvbv5->scan->lnb );
	if ( dvbv5->scan->sat_num >= 0 ) parms->sat_number = dvbv5->scan->sat_num;
	parms->diseqc_wait = dvbv5->scan->diseqc_wait;
	parms->freq_bpf = 0; // dvbv5->scan->freq_bpf;
	parms->lna = dvbv5->scan->lna;

	return TRUE;
}

static gboolean zap_parse ( const char *file, const char *channel, uint8_t frm, struct dvb_v5_fe_parms *parms, uint16_t pids[], uint16_t apids[], GtkWindow *window )
{
	struct dvb_file *dvb_file;
	struct dvb_entry *entry;

	uint8_t i = 0, j = 0;
	uint32_t sys = _get_delsys ( parms );

	dvb_file = dvb_read_file_format ( file, sys, frm );

	if ( !dvb_file )
	{
		g_critical ( "%s:: Read file format failed.", __func__ );
		return FALSE;
	}

	for ( entry = dvb_file->first_entry; entry != NULL; entry = entry->next )
	{
		if ( entry->channel && !strcmp ( entry->channel, channel ) ) break;
		if ( entry->vchannel && !strcmp ( entry->vchannel, channel ) ) break;
	}

	if ( !entry )
	{
		for ( entry = dvb_file->first_entry; entry != NULL; entry = entry->next )
		{
			if ( entry->channel && !strcasecmp ( entry->channel, channel ) ) break;
		}
	}

	if ( !entry )
	{
		uint32_t f, freq = atoi ( channel );

		if ( freq )
		{
			for ( entry = dvb_file->first_entry; entry != NULL; entry = entry->next )
			{
				dvb_retrieve_entry_prop ( entry, DTV_FREQUENCY, &f );

				if ( f == freq ) break;
			}
		}
	}

	if ( !entry )
	{
		g_critical ( "%s:: Can't find channel.", __func__ );
		dvbv5_message_dialog ( channel, "Can't find channel.", GTK_MESSAGE_ERROR, window );

		dvb_file_free ( dvb_file );
		return FALSE;
	}

	if ( entry->lnb )
	{
		int lnb = dvb_sat_search_lnb ( entry->lnb );

		if ( lnb == -1 )
		{
			g_warning ( "%s:: Unknown LNB %s", __func__, entry->lnb );
			dvb_file_free ( dvb_file );
			return FALSE;
		}

		parms->lnb = dvb_sat_get_lnb (lnb);
	}

	// pids[6];  0 - sid, 1 - vpid, 2 - apid, 3 - apid_len, 4 - sid found, 5 - vpid found
	if ( entry->service_id ) { pids[0] = entry->service_id;   pids[4] = 1; }
	if ( entry->video_pid  ) { pids[1] = entry->video_pid[0]; pids[5] = 1; }
	if ( entry->sat_number >= 0 ) parms->sat_number = entry->sat_number;

	if ( entry->audio_pid )
	{
		for ( j = 0; j < entry->audio_pid_len; j++ )
		{
			if ( j >= MAX_AUDIO ) { g_print ( "%s:: MAX_AUDIO %d: add apid stop \n", __func__, MAX_AUDIO ); break; }

			apids[j] = entry->audio_pid[j];
		}

		if ( j >= MAX_AUDIO ) pids[3] = MAX_AUDIO; else pids[3] = entry->audio_pid_len;
	}

	dvb_retrieve_entry_prop (entry, DTV_DELIVERY_SYSTEM, &sys );
	dvb_set_compat_delivery_system ( parms, sys );

	/* Copy data into parms */
	for ( i = 0; i < entry->n_props; i++ )
	{
		uint32_t data = entry->props[i].u.data;

		/* Don't change the delivery system */
		if ( entry->props[i].cmd == DTV_DELIVERY_SYSTEM ) continue;

		dvb_fe_store_parm ( parms, entry->props[i].cmd, data );

		if ( parms->current_sys == SYS_ISDBT )
		{
			dvb_fe_store_parm ( parms, DTV_ISDBT_PARTIAL_RECEPTION,  0 );
			dvb_fe_store_parm ( parms, DTV_ISDBT_SOUND_BROADCASTING, 0 );
			dvb_fe_store_parm ( parms, DTV_ISDBT_LAYER_ENABLED,   0x07 );

			if ( entry->props[i].cmd == DTV_CODE_RATE_HP )
			{
				dvb_fe_store_parm ( parms, DTV_ISDBT_LAYERA_FEC, data );
				dvb_fe_store_parm ( parms, DTV_ISDBT_LAYERB_FEC, data );
				dvb_fe_store_parm ( parms, DTV_ISDBT_LAYERC_FEC, data );
			}
			else if ( entry->props[i].cmd == DTV_MODULATION )
			{
				dvb_fe_store_parm ( parms, DTV_ISDBT_LAYERA_MODULATION, data );
				dvb_fe_store_parm ( parms, DTV_ISDBT_LAYERB_MODULATION, data );
				dvb_fe_store_parm ( parms, DTV_ISDBT_LAYERC_MODULATION, data );
			}
		}

		if ( parms->current_sys == SYS_ATSC && entry->props[i].cmd == DTV_MODULATION )
		{
			if ( data != VSB_8 && data != VSB_16 )
				dvb_fe_store_parm ( parms, DTV_DELIVERY_SYSTEM, SYS_DVBC_ANNEX_B );
		}
	}

	dvb_file_free ( dvb_file );

	return TRUE;
}

static uint32_t dvbv5_setup_frontend ( struct dvb_v5_fe_parms *parms )
{
	uint32_t freq = 0;

	int rc = dvb_fe_retrieve_parm ( parms, DTV_FREQUENCY, &freq );

	if ( rc < 0 ) return 0;

	rc = dvb_fe_set_parms ( parms );

	if ( rc < 0 ) return 0;

	return freq;
}

static gboolean dvbv5_zap_set_pes_filter ( struct dvb_open_descriptor *fd, uint16_t pid, dmx_pes_type_t type, dmx_output_t dmx, uint32_t buf_size, const char *type_name )
{
	if ( verbose ) g_message ( "  %s: DMX_SET_PES_FILTER %d (0x%04x)", type_name, pid, pid );

	if ( dvb_dev_dmx_set_pesfilter ( fd, pid, type, dmx, buf_size ) < 0 ) return FALSE;

	return TRUE;
}

static gboolean dvbv5_add_pat_pmt ( const char *demux_dev, uint16_t sid, uint8_t demux_descr, uint32_t bsz, Dvbv5 *dvbv5 )
{
	int pmtpid = 0;

	struct dvb_open_descriptor *sid_fd;
	sid_fd = dvb_dev_open ( dvbv5->dvb_zap, demux_dev, O_RDWR );

	if ( sid_fd )
	{
		pmtpid = dvb_dev_dmx_get_pmt_pid ( sid_fd, sid );
		dvb_dev_close ( sid_fd );
	}
	else
		return FALSE;

	dvbv5->pat_fd = dvb_dev_open ( dvbv5->dvb_zap, demux_dev, O_RDWR );

	if ( dvbv5->pat_fd )
		dvbv5_zap_set_pes_filter ( dvbv5->pat_fd, 0, DMX_PES_OTHER, demux_descr, bsz, "PAT" );
	else
		return FALSE;

	dvbv5->pmt_fd = dvb_dev_open ( dvbv5->dvb_zap, demux_dev, O_RDWR );

	if ( dvbv5->pmt_fd )
		dvbv5_zap_set_pes_filter ( dvbv5->pmt_fd, pmtpid, DMX_PES_OTHER, demux_descr, bsz, "PMT" );
	else
		return FALSE;

	return TRUE;
}

static void dvbv5_dvb_zap_set_dmx ( Dvbv5 *dvbv5 )
{
	int num_dmx = gtk_combo_box_get_active ( GTK_COMBO_BOX ( dvbv5->zap->combo_dmx ) );

	if ( out_demux_n[num_dmx].descr_num == DMX_OUT_OFF ) return;

	if ( out_demux_n[num_dmx].descr_num == DMX_OUT_ALL_PIDS )
	{
		dvbv5->video_fd = dvb_dev_open ( dvbv5->dvb_zap, dvbv5->demux_dev, O_RDWR );

		if ( dvbv5->video_fd )
			dvbv5_zap_set_pes_filter ( dvbv5->video_fd, 0x2000, DMX_PES_OTHER, DMX_OUT_TS_TAP, 64 * 1024, "ALL" );
		else
			g_critical ( "%s:: %s: failed opening %s", __func__, out_demux_n[num_dmx].name, dvbv5->demux_dev );

		return;
	}

	uint32_t bsz = ( out_demux_n[num_dmx].descr_num == DMX_OUT_TS_TAP || out_demux_n[num_dmx].descr_num == DMX_OUT_ALL_PIDS ) ? 64 * 1024 : 0;

	if ( dvbv5->pids[4] ) 
		dvbv5_add_pat_pmt ( dvbv5->demux_dev, dvbv5->pids[0], out_demux_n[num_dmx].descr_num, bsz, dvbv5 );
	// else /* uncomment for SID = 0 */
		// dvbv5_add_pat_pmt ( dvbv5->demux_dev, 0, out_demux_n[num_dmx].descr_num, bsz, dvbv5 );

	if ( dvbv5->pids[5] )
	{
		dvbv5->video_fd = dvb_dev_open ( dvbv5->dvb_zap, dvbv5->demux_dev, O_RDWR );

		if ( dvbv5->video_fd )
			dvbv5_zap_set_pes_filter ( dvbv5->video_fd, dvbv5->pids[1], DMX_PES_VIDEO, out_demux_n[num_dmx].descr_num, bsz, "VIDEO" );
		else
			g_critical ( "%s:: VIDEO: failed opening %s", __func__, dvbv5->demux_dev );
	}

	if ( dvbv5->pids[3] )
	{
		uint8_t j = 0; for ( j = 0; j < dvbv5->pids[3]; j++ )
		{
			dvbv5->audio_fds[j] = dvb_dev_open ( dvbv5->dvb_zap, dvbv5->demux_dev, O_RDWR );

			if ( dvbv5->audio_fds[j] )
				dvbv5_zap_set_pes_filter ( dvbv5->audio_fds[j], dvbv5->apids[j], ( j == 0 ) ? DMX_PES_AUDIO : DMX_PES_OTHER, out_demux_n[num_dmx].descr_num, bsz, "AUDIO" );
			else
				g_critical ( "%s:: AUDIO: failed opening %s", __func__, dvbv5->demux_dev );
		}
	}
}

static gboolean dvbv5_dvb_zap_check_fe_lock ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->window  == NULL ) return FALSE;
	if ( dvbv5->dvb_zap == NULL ) return FALSE;

	static uint32_t time = 1;
	struct dvb_v5_fe_parms *parms = dvbv5->dvb_zap->fe_parms;

	dvb_fe_get_stats ( parms ); // error is ignored

	fe_status_t status;
	dvb_fe_retrieve_stats ( parms, DTV_STATUS, &status );

	gboolean fe_lock = FALSE;
	if ( status & FE_HAS_LOCK ) fe_lock = TRUE;
	dvbv5->fe_lock = fe_lock;

	if ( fe_lock ) dvbv5_dvb_zap_set_dmx ( dvbv5 );
	if ( fe_lock ) { time = 1; return FALSE; }

	if ( dvbv5->debug ) g_message ( "%s: FE_HAS_LOCK failed ( %d milisecond ) ", __func__, time * 250 );
	time++;

	return TRUE;
}

static void dvbv5_dvb_zap_auto_free ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->debug ) g_message ( "%s: %s ", __func__, ( dvbv5->dvb_zap ) ? "True" : "False" );

	if ( dvbv5->dvb_zap )
	{
		if ( dvbv5->pat_fd ) dvb_dev_close ( dvbv5->pat_fd );
		if ( dvbv5->pmt_fd ) dvb_dev_close ( dvbv5->pmt_fd );
		if ( dvbv5->video_fd ) dvb_dev_close ( dvbv5->video_fd );

		dvbv5->pat_fd = NULL;
		dvbv5->pmt_fd = NULL;
		dvbv5->video_fd = NULL;

		uint8_t j = 0; for ( j = 0; j < dvbv5->pids[3]; j++ )
		{
			if ( dvbv5->audio_fds[j] ) dvb_dev_close ( dvbv5->audio_fds[j] );
			dvbv5->audio_fds[j] = NULL;
		}

		for ( j = 0; j < 6; j++ ) dvbv5->pids[j] = 0;

		dvb_dev_free ( dvbv5->dvb_zap );
		dvbv5->dvb_zap = NULL;
		dvbv5->demux_dev = NULL;

		gtk_label_set_text ( dvbv5->label_freq, "Freq: " );
	}
}

gboolean _dvbv5_dvb_zap_init ( const char *channel, Dvbv5 *dvbv5 )
{
	if ( !g_file_test ( dvbv5->zap_file, G_FILE_TEST_EXISTS ) )
	{
		dvbv5_message_dialog ( dvbv5->zap_file, g_strerror ( errno ), GTK_MESSAGE_ERROR, dvbv5->window );
		return FALSE;
	}

	dvbv5_dvb_zap_auto_free ( dvbv5 );
	dvbv5->dvb_zap = dvb_dev_alloc ();

	if ( !dvbv5->dvb_zap ) return FALSE;

	dvb_dev_set_log ( dvbv5->dvb_zap, 0, NULL );
	dvb_dev_find ( dvbv5->dvb_zap, NULL, NULL );
	struct dvb_v5_fe_parms *parms = dvbv5->dvb_zap->fe_parms;

	struct dvb_dev_list *dvb_dev = dvb_dev_seek_by_adapter ( dvbv5->dvb_zap, dvbv5->scan->adapter, dvbv5->scan->demux, DVB_DEVICE_DEMUX );

	if ( !dvb_dev )
	{
		g_critical ( "%s: Couldn't find demux device node.", __func__ );
		dvbv5_message_dialog ( "DVB_DEVICE_DEMUX", "Couldn't find demux device.", GTK_MESSAGE_ERROR, dvbv5->window );

		return FALSE;
	}

	dvbv5->demux_dev = dvb_dev->sysname;

	dvb_dev = dvb_dev_seek_by_adapter ( dvbv5->dvb_zap, dvbv5->scan->adapter, dvbv5->scan->demux, DVB_DEVICE_DVR );
	if ( !dvb_dev ) return FALSE;

	dvb_dev = dvb_dev_seek_by_adapter ( dvbv5->dvb_zap, dvbv5->scan->adapter, dvbv5->scan->frontend, DVB_DEVICE_FRONTEND );
	if ( !dvb_dev ) return FALSE;

	if ( !dvb_dev_open ( dvbv5->dvb_zap, dvb_dev->sysname, O_RDWR ) )
	{
		perror ( "Opening device failed" );
		dvbv5_message_dialog ( "DVB_DEVICE_FRONTEND", "Opening device failed.", GTK_MESSAGE_ERROR, dvbv5->window );

		return FALSE;
	}

	// pids[6];  0 - sid, 1 - vpid, 2 - apid, 3 - apid_len, 4 - sid found, 5 - vpid found
	uint8_t j = 0;
	for ( j = 0; j < 6; j++ ) dvbv5->pids[j] = 0;
	for ( j = 0; j < MAX_AUDIO; j++ ) { dvbv5->apids[j] = 0; dvbv5->audio_fds[j] = NULL; }

	parms->diseqc_wait = 0;
	parms->freq_bpf = 0;
	parms->lna = LNA_AUTO;

	if ( !zap_parse ( dvbv5->zap_file, channel, FILE_DVBV5, parms, dvbv5->pids, dvbv5->apids, dvbv5->window ) )
	{
		g_critical ( "%s:: Zap parse failed.", __func__ );
		return FALSE;
	}

	uint32_t freq = dvbv5_setup_frontend ( parms );

	if ( freq )
	{
		dvbv5->zap->zap_status = TRUE;
		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->combo_dmx ), FALSE );

		gboolean sat = FALSE;
		if ( dvb_fe_is_satellite ( parms->current_sys ) ) sat = TRUE;

		dvbv5->freq_scan = ( sat ) ? freq / 1000 : freq / 1000000;

		if ( dvbv5->debug ) g_message ( "%s:: Zap Ok.", __func__ );
	}
	else
	{
		g_warning ( "%s:: Zap failed.", __func__ );
		dvbv5_message_dialog ( "", "Zap failed.", GTK_MESSAGE_ERROR, dvbv5->window );

		return FALSE;
	}

	g_timeout_add ( 250, (GSourceFunc)dvbv5_dvb_zap_check_fe_lock, dvbv5 );

	return TRUE;
}

static void dvbv5_stats_update_freq ( GtkLabel *label, uint32_t freq, uint32_t progs, const char *set_label, const char *mhz )
{
	char *text = ( progs ) ? g_strdup_printf ( "%s %d %s  ...  [%d]", set_label, freq, mhz, progs )
                           : g_strdup_printf ( "%s %d %s", set_label, freq, mhz );

	gtk_label_set_text ( label, text );

	free ( text );
}

#ifndef LIGHT
static ulong file_query_info_uint ( const char *file_path, const char *query_info, const char *attribute )
{
	GFile *file = g_file_new_for_path ( file_path );

	GFileInfo *file_info = g_file_query_info ( file, query_info, 0, NULL, NULL );

	ulong out = ( file_info ) ? g_file_info_get_attribute_uint64 ( file_info, attribute  ) : 0;

	g_object_unref ( file );
	if ( file_info ) g_object_unref ( file_info );

	return out;
}

static void dvbv5_set_file_size ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->record == NULL ) return;

	char *file_rec = NULL;
	g_object_get ( dvbv5->record->file_sink, "location", &file_rec, NULL );

	if ( file_rec == NULL ) return;

	ulong dsize = file_query_info_uint ( file_rec, "standard::*", "standard::size" );

	char *str_size = g_format_size ( dsize );

	if ( dvbv5->rec_cnt <=  3 ) dvbv5->rec_lr =  TRUE;
	if ( dvbv5->rec_cnt >= 15 ) dvbv5->rec_lr = FALSE;

	char *markup = g_markup_printf_escaped ( "  %s <span foreground=\"#%xf0000\"> ◉ </span>", str_size, dvbv5->rec_cnt );

		gtk_label_set_markup ( dvbv5->label_rec, markup );

	free ( markup );

	g_free ( str_size );
	g_free ( file_rec );

	if ( dvbv5->rec_lr ) dvbv5->rec_cnt++; else dvbv5->rec_cnt--;
}

static void dvbv5_set_status_server ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->server == NULL ) return;

	uint num_cl = tcpserver_get ( dvbv5->server );

	g_autofree char *text = g_strdup_printf ( "Clients:  %d", num_cl );

	gtk_label_set_markup ( dvbv5->label_rec, text );
}
#endif

static void dvbv5_set_status ( uint32_t freq, Dvbv5 *dvbv5 )
{
	gboolean sat = FALSE;
	if ( dvb_fe_is_satellite ( dvbv5->dvb_fe->fe_parms->current_sys ) ) sat = TRUE;

	if ( dvbv5->freq_scan > 0 )
		dvbv5_stats_update_freq ( dvbv5->label_freq, dvbv5->freq_scan, dvbv5->progs_scan, "Freq:  ", "MHz" );
	else
		dvbv5_stats_update_freq ( dvbv5->label_freq, ( sat ) ? freq / 1000 : freq / 1000000, 0, "L-Freq:  ", "MHz" );

#ifndef LIGHT
	if ( dvbv5->zap->stm_status )
	{
		dvbv5_set_status_server ( dvbv5 );
	}
	else if ( dvbv5->zap->rec_status )
	{
		dvbv5_set_file_size ( dvbv5 );
	}
	else
		gtk_label_set_text ( dvbv5->label_rec, "" );
#endif
}

static void dvbv5_dvb_fe_run_beep ( uint8_t sgl_snr )
{
	uint16_t set = 500;

	if ( sgl_snr > 25 ) set = 1000;
	if ( sgl_snr > 50 ) set = 2000;
	if ( sgl_snr > 75 ) set = 3000;

	char *text = g_strdup_printf ( "speaker-test -t sine -f %d -l 1 & sleep .25 && kill -9 $!", set );
	// char *text = g_strdup_printf ( "gst-launch-1.0 audiotestsrc freq=%d ! audioconvert ! autoaudiosink & sleep .4 && kill -9 $!", set );
		system ( text );
	free ( text );
}

static void dvbv5_dvb_fe_beep ( uint16_t sgl, uint16_t snr, Dvbv5 *dvbv5 )
{
	if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON ( dvbv5->toggle_be ) ) )
	{
		time_t t_cur;
		time ( &t_cur );

		if ( ( t_cur - dvbv5->t_fe_start ) >= 3 )
		{
			time ( &dvbv5->t_fe_start );

			dvbv5_dvb_fe_run_beep ( sgl );
			g_usleep ( 250000 );
			dvbv5_dvb_fe_run_beep ( snr );
		}
	}
}

static char * dvbv5_dvb_fe_get_stats_layer_str ( struct dvb_v5_fe_parms *parms, uint8_t cmd, const char *name, uint8_t layer, gboolean debug )
{
	char *ret = NULL;

	struct dtv_stats *stat = dvb_fe_retrieve_stats_layer ( parms, cmd, layer );

	if ( !stat || stat->scale == FE_SCALE_NOT_AVAILABLE ) return ret;

	const char *global = ( layer ) ? "Layer" : "Global";

	switch ( stat->scale )
	{
		case FE_SCALE_DECIBEL:
			if ( cmd == DTV_STAT_SIGNAL_STRENGTH )
				ret = g_strdup_printf ( "%s %.2fdBm", name, stat->svalue / 1000. );
			else
				ret = g_strdup_printf ( "%s %.2fdB", name, stat->svalue / 1000. );
			break;
		case FE_SCALE_RELATIVE:
				ret = g_strdup_printf ( "%s %3.0f%%", name, ( 100 * stat->uvalue ) / 65535. );
			break;
		default:
			break;
	}

	if ( debug ) g_message ( "%s: %s | Decibel %lld | Relative %llu ", global, ret, stat->svalue, stat->uvalue );

	return ret;
}

static float dvbv5_dvb_fe_get_stats_layer_digits ( struct dvb_v5_fe_parms *parms, uint8_t cmd, const char *name, uint8_t layer, gboolean debug )
{
	float ret = 0;

	struct dtv_stats *stat = dvb_fe_retrieve_stats_layer ( parms, cmd, layer );

	if ( !stat || stat->scale == FE_SCALE_NOT_AVAILABLE ) return ret;

	const char *global = ( layer ) ? "Layer" : "Global";

	switch ( stat->scale )
	{
		case FE_SCALE_DECIBEL:
			if ( cmd == DTV_STAT_SIGNAL_STRENGTH )
				ret = stat->svalue / 1000.;
			else
				ret = stat->svalue / 1000.;
			break;
		case FE_SCALE_RELATIVE:
				ret = ( 100 * stat->uvalue ) / 65535.;
			break;
		default:
			break;
	}

	if ( debug ) g_message ( "%s %s: %.2f%% | Decibel %lld | Relative %llu ", global, name, ret, stat->svalue, stat->uvalue );

	return ret;
}

static uint8_t dvbv5_dvb_fe_get_manual_quality ( struct dvb_v5_fe_parms *parms, uint16_t vpid )
{
	uint8_t qual = 0;
	enum fecap_scale_params scale;
	float ber = dvb_fe_retrieve_ber ( parms, 0, &scale );

	// For the highest accuracy, you need to determine the stream rate ( set minimum - SD ):
	uint32_t allow_bits_error_good = 200, allow_bits_error_ok = 2000;
	if ( !vpid ) allow_bits_error_good = 10, allow_bits_error_ok = 100; // Radio

	if ( ber >  allow_bits_error_ok   ) qual = 1; // Poor ( BER > 1e−3: 1 error per 1000  bits )
	if ( ber <= allow_bits_error_ok   ) qual = 2; // 1e-4 > Ok < 1e-3 ( allow-bits-error )
	if ( ber <= allow_bits_error_good ) qual = 3; // Good ( BER < 1e-4: 1 error per 10000 bits )

	return qual;
}

static gboolean dvbv5_dvb_fe_get_stats ( Dvbv5 *dvbv5 )
{
	struct dvb_v5_fe_parms *parms = dvbv5->dvb_fe->fe_parms;

	int rc = dvb_fe_get_stats ( parms );

	if ( rc )
	{
		g_warning ( "%s:: failed.", __func__ );
		return FALSE;
	}

	uint32_t freq = 0, qual = 0;
	gboolean fe_lock = FALSE;

	fe_status_t status;
	dvb_fe_retrieve_stats ( parms, DTV_STATUS,  &status );
	dvb_fe_retrieve_stats ( parms, DTV_QUALITY,   &qual );
	dvb_fe_retrieve_parm  ( parms, DTV_FREQUENCY, &freq );

	if ( status & FE_HAS_LOCK ) fe_lock = TRUE;
	if ( qual == 0 ) qual = dvbv5_dvb_fe_get_manual_quality ( parms, dvbv5->pids[1] );
	if ( !fe_lock || !dvbv5->zap->zap_status ) qual = 0;

	uint8_t layer = 0;
	char *sgl_gs = NULL, *snr_gs = NULL;
	float sgl_gd = 0, snr_gd = 0;

	sgl_gs = dvbv5_dvb_fe_get_stats_layer_str ( parms, DTV_STAT_SIGNAL_STRENGTH, "Signal", layer, dvbv5->debug );
	snr_gs = dvbv5_dvb_fe_get_stats_layer_str ( parms, DTV_STAT_CNR, "Snr", layer, dvbv5->debug );

	if ( sgl_gs && g_strrstr ( sgl_gs, "dB" ) ) layer = 1;

	sgl_gd = dvbv5_dvb_fe_get_stats_layer_digits ( parms, DTV_STAT_SIGNAL_STRENGTH, "Signal", layer, dvbv5->debug );
	snr_gd = dvbv5_dvb_fe_get_stats_layer_digits ( parms, DTV_STAT_CNR, "Snr", layer, dvbv5->debug );

	if ( sgl_gs || snr_gs )
	{
		dvbv5_set_status  ( freq, dvbv5 );
		if ( !layer ) dvbv5_dvb_fe_beep ( sgl_gd, snr_gd, dvbv5 );
		level_set_sgn_snr ( qual, ( sgl_gs ) ? sgl_gs : "Signal", ( snr_gs ) ? snr_gs : "Snr", sgl_gd, snr_gd, fe_lock, dvbv5->level );
	}
	else
	{
		gtk_label_set_text ( dvbv5->label_rec, "" );
		gtk_label_set_text ( dvbv5->label_freq, "Freq: " );
		level_set_sgn_snr  ( 0, "Signal", "Snr", 0, 0, FALSE, dvbv5->level );
	}

	free ( sgl_gs );
	free ( snr_gs );

/*
	uint32_t sgl = 0, snr = 0;
	dvb_fe_retrieve_stats ( parms, DTV_STAT_CNR, &snr );
	dvb_fe_retrieve_stats ( parms, DTV_STAT_SIGNAL_STRENGTH, &sgl );
*/
	return TRUE;
}

static void dvbv5_dvb_fe_auto_free ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->debug ) g_message ( "%s: %s ", __func__, ( dvbv5->dvb_fe ) ? "True" : "False" );

	if ( dvbv5->dvb_fe )
	{
		dvb_dev_free ( dvbv5->dvb_fe );
		dvbv5->dvb_fe = NULL;
	}
}

static gboolean dvbv5_dvb_fe_show_stats ( Dvbv5 *dvbv5 )
{
	if ( dvbv5->window == NULL ) return FALSE;

	if ( dvbv5->stat_stop && dvbv5->thread_save )
	{
		if ( dvbv5->dvb_zap == NULL ) dvbv5->freq_scan  = 0;
		dvbv5->progs_scan = 0;

		dvbv5_dvb_scan_auto_free ( dvbv5 );
		dvbv5_dvb_fe_auto_free ( dvbv5 );

		if ( dvbv5->window )
		{
			level_set_sgn_snr  ( 0, "Signal", "Snr", 0, 0, FALSE, dvbv5->level );
			gtk_label_set_text ( dvbv5->label_freq, "Freq: " );
			gtk_label_set_text ( dvbv5->label_rec, "" );

			control_button_set_sensitive ( "start", TRUE, dvbv5->control );
			gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->scan ), TRUE );
			gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->toggle_fe  ), TRUE );
			gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( dvbv5->toggle_fe ), FALSE );
		}

		return FALSE;
	}

	// the returned result is ignored
	if ( dvbv5->dvb_fe ) dvbv5_dvb_fe_get_stats ( dvbv5 );

	return TRUE;
}

static gboolean _dvbv5_dvb_fe_stat_init ( Dvbv5 *dvbv5 )
{
	dvbv5_dvb_fe_auto_free ( dvbv5 );
	dvbv5->dvb_fe = dvb_dev_alloc ();

	if ( !dvbv5->dvb_fe ) return FALSE;

	dvb_dev_set_log ( dvbv5->dvb_fe, 0, NULL );
	dvb_dev_find ( dvbv5->dvb_fe, NULL, NULL );

	struct dvb_dev_list *dvb_dev_fe = dvb_dev_seek_by_adapter ( dvbv5->dvb_fe, dvbv5->scan->adapter, dvbv5->scan->frontend, DVB_DEVICE_FRONTEND );

	if ( !dvb_dev_fe )
	{
		g_critical ( "%s: Couldn't find demux device.", __func__ );
		return FALSE;
	}

	if ( !dvb_dev_open ( dvbv5->dvb_fe, dvb_dev_fe->sysname, O_RDONLY ) )
	{
		g_critical ( "%s: Opening device failed.", __func__ );
		return FALSE;
	}

	struct dvb_v5_fe_parms *parms = dvbv5->dvb_fe->fe_parms;

	dvbv5->cur_sys = parms->current_sys;
	gtk_label_set_text ( dvbv5->dvb_name, parms->info.name );

	return TRUE;
}

void dvbv5_get_dvb_info ( Dvbv5 *dvbv5 )
{
	if ( _dvbv5_dvb_fe_stat_init ( dvbv5 ) )
		dvbv5_dvb_fe_auto_free ( dvbv5 );
	else
		gtk_label_set_text ( dvbv5->dvb_name, "Undefined" );
}

static void dvbv5_clicked_stop ( Dvbv5 *dvbv5 )
{
#ifndef LIGHT
	if ( dvbv5->player ) player_stop ( dvbv5->player );
	if ( dvbv5->record ) record_stop ( dvbv5->record );
	if ( dvbv5->server ) tcpserver_stop ( dvbv5->server );
#endif

	dvbv5->zap->zap_status = FALSE;
	dvbv5_dvb_zap_auto_free ( dvbv5 );

	if ( dvbv5->window ) zap_stop_toggled_all ( dvbv5->zap );
	if ( dvbv5->window ) gtk_window_set_title ( dvbv5->window, PROGNAME );

	dvbv5->thread_stop = TRUE;
	dvbv5->stat_stop   = TRUE;

	dvbv5->freq_scan  = 0;
	dvbv5->progs_scan = 0;

	gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->zap->combo_dmx ), TRUE );
}

static void dvbv5_clicked_start ( Dvbv5 *dvbv5 )
{
	if ( !g_file_test ( dvbv5->input_file, G_FILE_TEST_EXISTS ) )
	{
		dvbv5_message_dialog ( dvbv5->input_file, g_strerror ( errno ), GTK_MESSAGE_ERROR, dvbv5->window );
		return;
	}

	if ( _dvbv5_dvb_scan_init ( dvbv5 ) )
	{
		dvbv5->freq_scan  = 0;
		dvbv5->progs_scan = 0;

		dvbv5->thread_save = FALSE;
		dvbv5->thread_stop = FALSE;

		dvbv5->thread = g_thread_new ( "scan-thread", (GThreadFunc)dvbv5_dvb_scan_thread_run, dvbv5 );
		g_thread_unref ( dvbv5->thread );

		if ( !gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON ( dvbv5->toggle_fe ) ) )
			gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( dvbv5->toggle_fe ), TRUE );

		control_button_set_sensitive ( "start", FALSE, dvbv5->control );
		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->scan ), FALSE );
		gtk_widget_set_sensitive ( GTK_WIDGET ( dvbv5->toggle_fe  ), FALSE );
	}
}

static void dvbv5_info ( Dvbv5 *dvbv5 )
{
	dvbv5_about ( dvbv5 );
}

static void dvbv5_dark ( G_GNUC_UNUSED Dvbv5 *dvbv5 )
{
	gboolean dark = FALSE;
	g_object_get ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", &dark, NULL );
	g_object_set ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", !dark, NULL );
	dvbv5->dark = !dark;
}

static void dvbv5_clicked_mini ( Dvbv5 *dvbv5 )
{
	if ( gtk_widget_is_visible ( GTK_WIDGET ( dvbv5->notebook ) ) )
		gtk_widget_hide ( GTK_WIDGET ( dvbv5->notebook ) );
	else
		gtk_widget_show ( GTK_WIDGET ( dvbv5->notebook ) );

	gtk_window_resize ( dvbv5->window, 300, 100 );
}

static void dvbv5_clicked_quit ( Dvbv5 *dvbv5 )
{
	gtk_widget_destroy ( GTK_WIDGET ( dvbv5->window ) );
}

static void dvbv5_button_clicked_handler ( G_GNUC_UNUSED Control *control, const char *button, Dvbv5 *dvbv5 )
{
	const char *b_n[] = { "start", "stop", "mini", /*"dark", "info",*/ "quit" };
	fp funcs[] = { dvbv5_clicked_start, dvbv5_clicked_stop, dvbv5_clicked_mini, /*dvbv5_dark, dvbv5_info,*/ dvbv5_clicked_quit };

	uint8_t c = 0; for ( c = 0; c < NUM_BUTTONS; c++ ) { if ( g_str_has_prefix ( b_n[c], button ) ) { funcs[c] ( dvbv5 ); break; } }
}

static void dvbv5_set_fe ( GtkToggleButton *button, Dvbv5 *dvbv5 )
{
	gboolean set_fe = gtk_toggle_button_get_active ( button );

	if ( set_fe )
	{
		if ( _dvbv5_dvb_fe_stat_init ( dvbv5 ) )
		{
			dvbv5->stat_stop = FALSE;
			g_timeout_add ( 250, (GSourceFunc)dvbv5_dvb_fe_show_stats, dvbv5 );
		}
		else
			gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( button ), FALSE );
	}
	else
		dvbv5->stat_stop = TRUE;
}

static GtkBox * dvbv5_create_info ( Dvbv5 *dvbv5 )
{
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	dvbv5->label_freq = (GtkLabel *)gtk_label_new ( "Freq: " );
	dvbv5->label_rec  = (GtkLabel *)gtk_label_new ( "" );

	dvbv5->toggle_fe = (GtkCheckButton *)gtk_check_button_new_with_label ( "Fe" );
	g_signal_connect ( dvbv5->toggle_fe, "toggled", G_CALLBACK ( dvbv5_set_fe ), dvbv5 );

	dvbv5->toggle_be = (GtkCheckButton *)gtk_check_button_new_with_label ( "Be" );
	gtk_widget_set_tooltip_text ( GTK_WIDGET ( dvbv5->toggle_be ), 
		"Beep 1 - Signal; Beep 2 - Snr\n0-25:  beep 500 Hz\n25-50: beep 1000 Hz\n50-75: beep 2000 Hz\n75-100: beep 3000 Hz" );

	gtk_box_pack_start ( h_box, GTK_WIDGET ( dvbv5->label_freq ), FALSE, FALSE, 0 );
#ifndef LIGHT
	gtk_box_pack_end   ( h_box, GTK_WIDGET ( dvbv5->toggle_be  ), FALSE, FALSE, 0 );
#endif
	gtk_box_pack_end   ( h_box, GTK_WIDGET ( dvbv5->toggle_fe  ), FALSE, FALSE, 0 );
	gtk_box_pack_end   ( h_box, GTK_WIDGET ( dvbv5->label_rec  ), FALSE, FALSE, 0 );

	return h_box;
}

static GtkBox * dvbv5_create_control_box ( Dvbv5 *dvbv5 )
{
	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( dvbv5_create_info ( dvbv5 ) ), FALSE, FALSE, 0 );

	dvbv5->level = level_new ();
	gtk_box_pack_start ( v_box, GTK_WIDGET ( dvbv5->level ), FALSE, FALSE, 5 );

	dvbv5->control = control_new ( dvbv5->icon_size );
	g_signal_connect ( dvbv5->control, "button-clicked", G_CALLBACK ( dvbv5_button_clicked_handler ), dvbv5 );
	gtk_box_pack_start ( v_box, GTK_WIDGET ( dvbv5->control ), FALSE, FALSE, 0 );

	return v_box;
}

static void dvbv5_header_bar_menu_about ( G_GNUC_UNUSED GtkButton *button, Dvbv5 *dvbv5 )
{
	gtk_widget_set_visible ( GTK_WIDGET ( dvbv5->popover ), FALSE );
	dvbv5_info ( dvbv5 );
}

static void dvbv5_header_bar_menu_dark ( G_GNUC_UNUSED GtkButton *button, Dvbv5 *dvbv5 )
{
	gtk_widget_set_visible ( GTK_WIDGET ( dvbv5->popover ), FALSE );
	dvbv5_dark ( dvbv5 );
}

static void dvbv5_header_bar_menu_quit ( G_GNUC_UNUSED GtkButton *button, Dvbv5 *dvbv5 )
{
	gtk_widget_set_visible ( GTK_WIDGET ( dvbv5->popover ), FALSE );
	gtk_widget_destroy ( GTK_WIDGET ( dvbv5->window ) );
}

static GtkHeaderBar * dvbv5_header_bar ( Dvbv5 *dvbv5 )
{
	GtkHeaderBar *header_bar = (GtkHeaderBar *)gtk_header_bar_new ();
	gtk_header_bar_set_show_close_button ( header_bar, TRUE );
	gtk_header_bar_set_title ( header_bar, PROGNAME );
	gtk_header_bar_set_has_subtitle ( header_bar, FALSE );

	GtkMenuButton *menu_button = (GtkMenuButton *)gtk_menu_button_new ();
	gtk_button_set_label ( GTK_BUTTON ( menu_button ), "𝋯" );

	dvbv5->popover = (GtkPopover *)gtk_popover_new ( GTK_WIDGET ( header_bar ) );
	gtk_popover_set_position ( dvbv5->popover, GTK_POS_TOP );
	gtk_container_set_border_width ( GTK_CONTAINER ( dvbv5->popover ), 10 );

	GtkBox *vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 5 );
	gtk_box_set_spacing ( vbox, 5 );

	struct Data { const char *icon_u; const char *name; void ( *f )(GtkButton *, Dvbv5 *); } data_n[] =
	{
		{ "⏾", "dvb-dark",  dvbv5_header_bar_menu_dark  },
		{ "🛈", "dvb-info",  dvbv5_header_bar_menu_about },
		{ "⏻", "dvb-quit",  dvbv5_header_bar_menu_quit  }
	};

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( data_n ); c++ )
	{
#ifndef LIGHT
		GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( data_n[c].name, GTK_ICON_SIZE_MENU );
#else
		GtkButton *button = (GtkButton *)gtk_button_new_with_label ( data_n[c].icon_u );
#endif
		g_signal_connect ( button, "clicked", G_CALLBACK ( data_n[c].f ), dvbv5 );
		gtk_widget_show ( GTK_WIDGET ( button ) );
		gtk_box_pack_start ( vbox, GTK_WIDGET ( button ), FALSE, FALSE, 0 );
	}

	gtk_container_add ( GTK_CONTAINER ( dvbv5->popover ), GTK_WIDGET ( vbox ) );
	gtk_widget_show ( GTK_WIDGET ( vbox ) );

	gtk_menu_button_set_popover ( menu_button, GTK_WIDGET ( dvbv5->popover ) );
	gtk_header_bar_pack_end ( header_bar, GTK_WIDGET ( menu_button ) );

	return header_bar;
}

static void dvbv5_window_quit ( G_GNUC_UNUSED GtkWindow *window, Dvbv5 *dvbv5 )
{
#ifndef LIGHT
	save_pref ( dvbv5 );
#endif

	dvbv5->window = NULL;

#ifndef LIGHT
	if ( dvbv5->player ) player_destroy ( dvbv5->player );
	if ( dvbv5->record ) record_destroy ( dvbv5->record );
	if ( dvbv5->server ) tcpserver_destroy ( dvbv5->server );
#endif

	dvbv5_dvb_scan_auto_free ( dvbv5 );
	dvbv5_dvb_zap_auto_free ( dvbv5 );
	dvbv5_dvb_fe_auto_free ( dvbv5 );

	free ( dvbv5->zap_file );
	free ( dvbv5->input_file );
	free ( dvbv5->output_file );
}

static void dvbv5_new_window ( GApplication *app )
{
	setlocale ( LC_NUMERIC, "C" );

	Dvbv5 *dvbv5 = DVBV5_APPLICATION ( app );

	gtk_icon_theme_add_resource_path ( gtk_icon_theme_get_default (), "/dvbv5" );

	dvbv5->window = (GtkWindow *)gtk_application_window_new ( GTK_APPLICATION ( app ) );
	gtk_window_set_title ( dvbv5->window, PROGNAME );
	gtk_window_set_icon_name ( dvbv5->window, "display" );
	g_signal_connect ( dvbv5->window, "destroy", G_CALLBACK ( dvbv5_window_quit ), dvbv5 );

	GdkPixbuf *pixbuf = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default (),
		"dvb-logo", 48, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );

	if ( pixbuf )
	{
		gtk_window_set_icon ( dvbv5->window, pixbuf );
		g_object_unref ( pixbuf );
	}
	else
		gtk_window_set_icon_name ( dvbv5->window, "display" );

	GtkHeaderBar *header_bar = dvbv5_header_bar ( dvbv5 );
	gtk_window_set_titlebar ( dvbv5->window, GTK_WIDGET ( header_bar ) );

	GtkBox *main_vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_set_spacing ( main_vbox, 5 );

	dvbv5->dvb_name = (GtkLabel *)gtk_label_new ( "Dvb Device" );
	gtk_box_pack_start ( main_vbox, GTK_WIDGET ( dvbv5->dvb_name ), FALSE, FALSE, 0 );

	dvbv5->scan = scan_new ();
	scan_signals ( dvbv5 );

	dvbv5->zap = zap_new ();
	zap_signals ( out_demux_n, G_N_ELEMENTS ( out_demux_n ), dvbv5 );

	GtkBox *c_box = dvbv5_create_control_box ( dvbv5 );
#ifndef LIGHT
	dvbv5->v_box_pref = create_pref ( dvbv5 );
#endif
	dvbv5->notebook = (GtkNotebook *)gtk_notebook_new ();
	gtk_notebook_set_scrollable ( dvbv5->notebook, TRUE );

	gtk_notebook_append_page ( dvbv5->notebook, GTK_WIDGET ( dvbv5->scan ), gtk_label_new ( "Scan" ) );
	gtk_notebook_append_page ( dvbv5->notebook, GTK_WIDGET ( dvbv5->zap  ), gtk_label_new ( "Zap"  ) );
#ifndef LIGHT
	gtk_notebook_append_page ( dvbv5->notebook, GTK_WIDGET ( dvbv5->v_box_pref ), gtk_label_new ( "⚒"  ) );
#endif
	gtk_notebook_set_tab_pos ( dvbv5->notebook, GTK_POS_TOP );
	gtk_box_pack_start ( main_vbox, GTK_WIDGET (dvbv5->notebook), TRUE, TRUE, 0 );

	gtk_box_pack_end ( main_vbox, GTK_WIDGET ( c_box ), FALSE, FALSE, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( main_vbox ), 10 );
	gtk_container_add   ( GTK_CONTAINER ( dvbv5->window ), GTK_WIDGET ( main_vbox ) );

	gtk_widget_show_all ( GTK_WIDGET ( dvbv5->window ) );

	dvbv5_get_dvb_info ( dvbv5 );

	dvbv5->connect = g_bus_get_sync ( G_BUS_TYPE_SESSION, NULL, NULL );

#ifndef LIGHT
	set_pref ( dvbv5 );
#endif
}

static void dvbv5_activate ( GApplication *app )
{
	dvbv5_new_window ( app );
}

static void dvbv5_init ( Dvbv5 *dvbv5 )
{
#ifndef LIGHT
	dvbv5->record = record_new ();
	dvbv5->server = tcpserver_new ();
	dvbv5->player = player_new ( TRUE );
#endif

	dvbv5->dvb_scan = NULL;
	dvbv5->dvb_fe   = NULL;
	dvbv5->dvb_zap  = NULL;

	dvbv5->pat_fd = NULL;
	dvbv5->pmt_fd = NULL;
	dvbv5->video_fd = NULL;

	dvbv5->cookie  = 0;
	dvbv5->cur_sys = 0;
	dvbv5->fe_lock = FALSE;
	dvbv5->thread_save = TRUE;

	dvbv5->input_format  = FILE_DVBV5;
	dvbv5->output_format = FILE_DVBV5;
	dvbv5->input_file    = g_strdup ( "Initial" );
	dvbv5->output_file   = g_strconcat ( g_get_home_dir (), "/dvb_channel.conf", NULL );
	dvbv5->zap_file      = g_strdup ( dvbv5->output_file );

	dvbv5->dark      = FALSE;
	dvbv5->opacity   = 100;
	dvbv5->icon_size = 20;

	dvbv5->debug = ( verbose ) ? TRUE : FALSE;

	dvbv5->settings = NULL;
#ifndef LIGHT
	load_pref ( dvbv5 );
#endif
}

static void dvbv5_finalize ( GObject *object )
{
	G_OBJECT_CLASS (dvbv5_parent_class)->finalize (object);
}

static void dvbv5_class_init ( Dvbv5Class *class )
{
	G_APPLICATION_CLASS (class)->activate = dvbv5_activate;

	G_OBJECT_CLASS (class)->finalize = dvbv5_finalize;
}

static Dvbv5 * dvbv5_new (void)
{
	return g_object_new ( DVBV5_TYPE_APPLICATION, /*"application-id", "org.gnome.dvbv5-gtk",*/ "flags", G_APPLICATION_FLAGS_NONE, NULL );
}

static void dvbv5_set_debug (void)
{
	const char *debug = g_getenv ( "DVB_DEBUG" );

	if ( debug ) verbose = atoi ( debug );
}

int main (void)
{
#ifndef LIGHT
	gst_init ( NULL, NULL );
#endif

	dvbv5_set_debug ();

	Dvbv5 *app = dvbv5_new ();

	int status = g_application_run ( G_APPLICATION (app), 0, NULL );

	g_object_unref (app);

	return status;
}
