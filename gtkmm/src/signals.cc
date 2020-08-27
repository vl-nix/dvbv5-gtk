/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "dvbv5.h"
#include <iostream>
#include <chrono>

#define MAX_AUDIO 32

extern "C"
{

#include <linux/dvb/dmx.h>
#include <libdvbv5/dvb-dev.h>
#include <libdvbv5/dvb-scan.h>
#include <libdvbv5/dvb-file.h>
#include <libdvbv5/dvb-demux.h>
#include <libdvbv5/dvb-v5-std.h>

#include <glib.h>

static uint8_t stop_thread = 0, verbose = 0;
static uint32_t freq_scan = 0;

struct dvb_device *dvb_zap_am = NULL;

struct ScanData
{
	uint8_t adapter, frontend, demux, time_mult;
	uint8_t new_freqs, get_detect, get_nit, other_nit, diseqc_wait;
	int8_t  lna, sat_num;
	const char *input_file, *output_file, *lnb_str;
};

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

static void dvb_scan ( struct ScanData *data )
{
	// Init

	struct dvb_device *dvb = dvb_dev_alloc ();

	if ( !dvb ) return;

	dvb_dev_set_log ( dvb, 0, NULL );
	dvb_dev_find ( dvb, NULL, NULL );

	struct dvb_dev_list *dvb_dev = dvb_dev_seek_by_adapter ( dvb, data->adapter, data->demux, DVB_DEVICE_DEMUX );

	if ( !dvb_dev ) { dvb_dev_free ( dvb ); return; }

	char *demux_dev = dvb_dev->sysname;
	g_message ( "%s:: scan->demux_dev: %s ", __func__, demux_dev );

	dvb_dev = dvb_dev_seek_by_adapter ( dvb, data->adapter, data->frontend, DVB_DEVICE_FRONTEND );

	if ( !dvb_dev ) { dvb_dev_free ( dvb ); return; }

	if ( !dvb_dev_open ( dvb, dvb_dev->sysname, O_RDWR ) ) { dvb_dev_free ( dvb ); return; }

	struct dvb_v5_fe_parms *parms = dvb->fe_parms;

	int8_t lnb = dvb_sat_search_lnb ( data->lnb_str );
	if ( lnb >= 0 ) parms->lnb = dvb_sat_get_lnb ( lnb );
	if ( data->sat_num >= 0 ) parms->sat_number = data->sat_num;
	parms->diseqc_wait = data->diseqc_wait;
	parms->lna = data->lna;
	parms->freq_bpf = 0;

	// Run

	struct dvb_file *dvb_file = NULL, *dvb_file_new = NULL;
	struct dvb_entry *entry;
	struct dvb_open_descriptor *dmx_fd;

	int stop = 0, count = 0, shift;
	uint32_t freq = 0, sys = _get_delsys ( parms );
	unsigned int pol = 0;

	dvb_file = dvb_read_file_format ( data->input_file, sys, FILE_DVBV5 );

	if ( !dvb_file )
	{
		dvb_dev_free ( dvb );
		g_critical ( "%s:: Read file format failed.", __func__ );
		return;
	}

	dmx_fd = dvb_dev_open ( dvb, demux_dev, O_RDWR );

	if ( !dmx_fd )
	{
		dvb_file_free ( dvb_file );
		dvb_dev_free ( dvb );

		perror ( "opening demux failed" );
		return;
	}

	GMutex mutex;
	g_mutex_init ( &mutex );

	for ( entry = dvb_file->first_entry; entry != NULL; entry = entry->next )
	{
		struct dvb_v5_descriptors *dvb_scan_handler = NULL;
		uint32_t stream_id;

		if ( dvb_retrieve_entry_prop ( entry, DTV_FREQUENCY, &freq ) ) continue;

		shift = dvb_estimate_freq_shift ( parms );

		if ( dvb_retrieve_entry_prop ( entry, DTV_POLARIZATION, &pol ) ) pol = POLARIZATION_OFF;
		if ( dvb_retrieve_entry_prop ( entry, DTV_STREAM_ID, &stream_id ) ) stream_id = NO_STREAM_ID_FILTER;
		if ( !dvb_new_entry_is_needed ( dvb_file->first_entry, entry, freq, shift, (enum dvb_sat_polarization)pol, stream_id ) ) continue;

		count++;
		dvb_log ( "Scanning frequency #%d %d", count, freq );

		gboolean sat = FALSE;
		if ( dvb_fe_is_satellite ( parms->current_sys ) ) sat = TRUE;

		g_mutex_lock ( &mutex );
			freq_scan = ( sat ) ? freq / 1000 : freq / 1000000;
		g_mutex_unlock ( &mutex );

		dvb_scan_handler = dvb_dev_scan ( dmx_fd, entry, &_check_frontend, NULL, data->other_nit, data->time_mult );

		g_mutex_lock ( &mutex );
			stop = stop_thread;
		g_mutex_unlock ( &mutex );

		if ( parms->abort || stop )
		{
			dvb_scan_free_handler_table ( dvb_scan_handler );
			break;
		}

		if ( !dvb_scan_handler ) continue;

		dvb_store_channel ( &dvb_file_new, parms, dvb_scan_handler, data->get_detect, data->get_nit );

		if ( !data->new_freqs ) dvb_add_scaned_transponders ( parms, dvb_scan_handler, dvb_file->first_entry, entry );

		dvb_scan_free_handler_table ( dvb_scan_handler );
	}

	if ( dvb_file_new ) dvb_write_file_format ( data->output_file, dvb_file_new, parms->current_sys, FILE_DVBV5 );

	dvb_file_free ( dvb_file );
	if ( dvb_file_new ) dvb_file_free ( dvb_file_new );

	dvb_dev_close ( dmx_fd );
	dvb_dev_free ( dvb );
	g_mutex_clear ( &mutex );
}

static bool zap_parse ( const char *file, const char *channel, struct dvb_v5_fe_parms *parms, uint16_t pids[], uint16_t apids[] )
{
	struct dvb_file *dvb_file;
	struct dvb_entry *entry;
	uint8_t i = 0, j = 0;
	uint32_t sys = _get_delsys ( parms );

	dvb_file = dvb_read_file_format ( file, sys, FILE_DVBV5 );

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

static bool dvbv5_zap_set_pes_filter ( struct dvb_open_descriptor *fd, uint16_t pid, dmx_pes_type_t type, dmx_output_t dmx, uint32_t buf_size, const char *type_name )
{
	if ( verbose ) g_message ( "  %s: DMX_SET_PES_FILTER %d (0x%04x)", type_name, pid, pid );

	if ( dvb_dev_dmx_set_pesfilter ( fd, pid, type, dmx, buf_size ) < 0 ) return FALSE;

	return TRUE;
}

static void dvb_zap_auto_free ()
{
	if ( dvb_zap_am ) { dvb_dev_free ( dvb_zap_am ); dvb_zap_am = NULL; }
}

static void dvb_zap ( const char *channel, const char *zap_file, int adapter, int frontend, int demux, const char *zap_dmx )
{
	dvb_zap_auto_free ();
	struct dvb_device *dvb = dvb_dev_alloc ();

	if ( !dvb ) return;
	dvb_zap_am = dvb;

	dvb_dev_set_log ( dvb, 0, NULL );
	dvb_dev_find ( dvb, NULL, NULL );

	struct dvb_v5_fe_parms *parms = dvb->fe_parms;

	struct dvb_dev_list *dvb_dev = dvb_dev_seek_by_adapter ( dvb, adapter, demux, DVB_DEVICE_DEMUX );

	if ( !dvb_dev )
	{
		g_critical ( "%s: Couldn't find demux device node.", __func__ );
		return;
	}

	char *demux_dev = dvb_dev->sysname;

	dvb_dev = dvb_dev_seek_by_adapter ( dvb, adapter, demux, DVB_DEVICE_DVR );
	if ( !dvb_dev ) return;

	dvb_dev = dvb_dev_seek_by_adapter ( dvb, adapter, frontend, DVB_DEVICE_FRONTEND );
	if ( !dvb_dev ) return;

	if ( !dvb_dev_open ( dvb, dvb_dev->sysname, O_RDWR ) )
	{
		perror ( "Opening device failed" );
		return;
	}

	// pids[6];  0 - sid, 1 - vpid, 2 - apid, 3 - apid_len, 4 - sid found, 5 - vpid found
	uint16_t pids[6];
	uint16_t apids[MAX_AUDIO];

	uint8_t j = 0;
	for ( j = 0; j < 6; j++ ) pids[j] = 0;
	for ( j = 0; j < MAX_AUDIO; j++ ) apids[j] = 0;

	parms->diseqc_wait = 0;
	parms->freq_bpf = 0;
	parms->lna = LNA_AUTO;

	if ( !zap_parse ( zap_file, channel, parms, pids, apids ) )
	{
		g_critical ( "%s:: Zap parse failed.", __func__ );

		return;
	}

	uint32_t freq = dvbv5_setup_frontend ( parms );

	if ( freq )
	{
		g_message ( "%s:: Zap Ok.", __func__ );
	}
	else
	{
		g_warning ( "%s:: Zap failed.", __func__ );

		return;
	}

	if ( verbose ) g_message ( "%s: demux %s ", __func__, zap_dmx );

	struct dvb_open_descriptor *sid_fd, *pat_fd, *pmt_fd, *video_fd, *audio_fds[MAX_AUDIO];

	if ( g_str_has_prefix ( "DMX_OUT_OFF", zap_dmx ) ) return;

	if ( g_str_has_prefix ( "DMX_OUT_ALL_PIDS", zap_dmx ) )
	{
		video_fd = dvb_dev_open ( dvb, demux_dev, O_RDWR );

		if ( video_fd )
			dvbv5_zap_set_pes_filter ( video_fd, 0x2000, DMX_PES_OTHER, DMX_OUT_TS_TAP, 64 * 1024, "ALL" );
		else
			g_critical ( "%s:: failed opening %s", __func__, demux_dev );

		return;
	}

	dmx_output_t dmx = DMX_OUT_TS_TAP;
	dmx_output_t dmx_n[] = { DMX_OUT_DECODER, DMX_OUT_TAP, DMX_OUT_TS_TAP, DMX_OUT_TSDEMUX_TAP };
	const char *d_n[] = { "DMX_OUT_DECODER", "DMX_OUT_TAP", "DMX_OUT_TS_TAP", "DMX_OUT_TSDEMUX_TAP" };
	uint8_t c = 0; for ( c = 0; c < 4; c++ ) { if ( g_str_has_prefix ( d_n[c], zap_dmx ) ) { dmx = dmx_n[c]; break; } }

	int pmtpid = 0;
	uint32_t bsz = ( g_str_has_prefix ( "DMX_OUT_TS_TAP", zap_dmx ) || g_str_has_prefix ( "DMX_OUT_ALL_PIDS", zap_dmx ) ) ? 64 * 1024 : 0;

	sid_fd = dvb_dev_open ( dvb, demux_dev, O_RDWR );

	if ( sid_fd )
	{
		pmtpid = dvb_dev_dmx_get_pmt_pid ( sid_fd, pids[0] );
		dvb_dev_close ( sid_fd );
	}
	else
		return;

	pat_fd = dvb_dev_open ( dvb, demux_dev, O_RDWR );

	if ( pat_fd )
		dvbv5_zap_set_pes_filter ( pat_fd, 0, DMX_PES_OTHER, dmx, bsz, "PAT" );
	else
		return;

	pmt_fd = dvb_dev_open ( dvb, demux_dev, O_RDWR );

	if ( pmt_fd )
		dvbv5_zap_set_pes_filter ( pmt_fd, pmtpid, DMX_PES_OTHER, dmx, bsz, "PMT" );
	else
		return;


	if ( pids[5] )
	{
		video_fd = dvb_dev_open ( dvb, demux_dev, O_RDWR );

		if ( video_fd )
			dvbv5_zap_set_pes_filter ( video_fd, pids[1], DMX_PES_VIDEO, dmx, bsz, "VIDEO" );
		else
			g_critical ( "%s:: VIDEO: failed opening %s", __func__, demux_dev );
	}

	if ( pids[3] )
	{
		uint8_t j = 0; for ( j = 0; j < pids[3]; j++ )
		{
			audio_fds[j] = dvb_dev_open ( dvb, demux_dev, O_RDWR );

			if ( audio_fds[j] )
				dvbv5_zap_set_pes_filter ( audio_fds[j], apids[j], ( j == 0 ) ? DMX_PES_AUDIO : DMX_PES_OTHER, dmx, bsz, "AUDIO" );
			else
				g_critical ( "%s:: AUDIO: failed opening %s", __func__, demux_dev );
		}
	}
}



char * dvb_get_name ( int fd )
{
	struct dvb_frontend_info info;

	if ( ( ioctl ( fd, FE_GET_INFO, &info ) ) == -1 )
	{
		perror ( "Ioctl:: FE_GET_INFO " );

		return g_strdup ( "Undefined" );
	}

	return g_strdup ( info.name );
}

uint8_t dvb_get_lock ( int fd )
{
	fe_status status;

	if ( ( ioctl ( fd, FE_READ_STATUS, &status ) ) == -1 ) return 0;

	if ( status & FE_HAS_LOCK ) return 1;

	return 0;
}

uint16_t dvb_get_sgl_snr ( int fd, uint32_t num )
{
	uint16_t data = 0;

	if ( ioctl ( fd, num, &data ) == -1 ) data = 0;

	return data;
}

int dvb_open_device ( uint8_t adapter, uint8_t frontend )
{
	g_autofree char *fd_name = g_strdup_printf ( "/dev/dvb/adapter%d/frontend%d", adapter, frontend );

	int fd = open ( fd_name, O_RDONLY, 0 );

	if ( fd == -1 ) { g_critical ( "%s: %s %s \n", __func__, g_strerror ( errno ), fd_name ); }

	return fd;
}

void dvb_close_device ( int fd )
{
	close ( fd );
}

}



void Dvbv5::thread_stop ()
{
	stop_thread = 1;
}

void Dvbv5::thread_start ()
{
	if ( debug ) std::cout << "Start thread" << std::endl;

	{
		std::lock_guard<std::mutex> lock(Mutex);

		stop_thread = 0;
		FinishThread = false;

		struct ScanData data = {};

		data.adapter     = scan->SpinButton[0]->get_value_as_int();
		data.frontend    = scan->SpinButton[1]->get_value_as_int();
		data.demux       = scan->SpinButton[2]->get_value_as_int();
		data.time_mult   = scan->SpinButton[3]->get_value_as_int();

		data.new_freqs   = ( scan->CheckButton[0]->get_active() ) ? 1 : 0;
		data.get_detect  = ( scan->CheckButton[1]->get_active() ) ? 1 : 0;
		data.get_nit     = ( scan->CheckButton[2]->get_active() ) ? 1 : 0;
		data.other_nit   = ( scan->CheckButton[3]->get_active() ) ? 1 : 0;

		int num 	  = scan->ComboLna->get_active_row_number();
		data.lna 	  = ( num == 2 ) ? -1 : num;
		//data.lnb 	  = scan->ComboLnb->get_active_row_number();
		data.sat_num     = scan->SpinButton[4]->get_value_as_int();
		data.diseqc_wait = scan->SpinButton[5]->get_value_as_int();

		//data.lna_str     = scan->ComboLna->get_active_id().c_str();
		data.lnb_str     = scan->ComboLnb->get_active_id().c_str();

		data.input_file  = input_file.c_str();
		data.output_file = output_file.c_str();

		dvb_scan ( &data );

		FinishThread = true;
	}
}

void Dvbv5::thread_delete ()
{
	if ( ScanThread && FinishThread )
	{
		if ( debug ) std::cout << "Delete thread" << std::endl;

		if ( ScanThread->joinable () ) ScanThread->join ();

		delete ScanThread;
		ScanThread = nullptr;

		Status->set_text ( "Status: " );
		level->level_set_sgl_snr ( 0, 0, false );
	}
}

bool Dvbv5::timeout_update ( uint num )
{
	if ( StopInfo ) return false;

	if ( ScanThread )
	{
		if ( stop_thread )
			Status->set_text ( "Status: Save" );
		else
		{
			Glib::ustring text = Glib::ustring::compose ( "Status: %1 MHz", freq_scan );
			Status->set_text ( text );
		}

		if ( FinishThread ) thread_delete ();
	}

	uint8_t lock = dvb_get_lock ( fd_info );
	uint16_t sgl = dvb_get_sgl_snr ( fd_info, FE_READ_SIGNAL_STRENGTH );
	uint16_t snr = dvb_get_sgl_snr ( fd_info, FE_READ_SNR );

	level->level_set_sgl_snr ( (sgl * 100) / 0xffff, (snr * 100) / 0xffff, ( lock ) ? true : false );

	return true;
}

void Dvbv5::update_device ()
{
	uint8_t a = scan->SpinButton[0]->get_value_as_int();
	uint8_t f = scan->SpinButton[1]->get_value_as_int();

	if ( fd_info >= 0 ) dvb_close_device ( fd_info );

	fd_info = dvb_open_device ( a, f );

	Glib::ustring name = dvb_get_name ( fd_info );

	Device->set_text ( name );
}

void Dvbv5::treeview_row_act ( const Gtk::TreeModel::Path &path, Gtk::TreeViewColumn * /*column*/ )
{
	if ( !Glib::file_test ( zap_file, Glib::FILE_TEST_EXISTS ) ) { message_dialog ( "File not found.", zap_file ); return; }

	Gtk::TreeModel::iterator iter = zap->TreeView->get_model()->get_iter ( path );

	if ( iter )
	{
		Gtk::TreeModel::Row row = *iter;
		Glib::ustring text = row[zap->Columns.col_ch];

		uint8_t adapter     = scan->SpinButton[0]->get_value_as_int();
		uint8_t frontend    = scan->SpinButton[1]->get_value_as_int();
		uint8_t demux       = scan->SpinButton[2]->get_value_as_int();

		verbose = ( debug ) ? 1 : 0;
		Status->set_text ( text );
		dvb_zap ( text.c_str(), zap_file.c_str(), adapter, frontend, demux, zap->ComboDemux->get_active_id().c_str() );
	}
}

void Dvbv5::icon_press_inp ( Gtk::EntryIconPosition icon_position, const GdkEventButton* event )
{
	Glib::ustring filename = open_file ();

	if ( !filename.empty() )
	{
		input_file = filename;
		scan->EntryInt->set_text ( Glib::path_get_basename ( filename ) );
	}
}

void Dvbv5::icon_press_out ( Gtk::EntryIconPosition icon_position, const GdkEventButton* event )
{
	Glib::ustring filename = save_file ();

	if ( !filename.empty() )
	{
		output_file = filename;
		scan->EntryOut->set_text ( Glib::path_get_basename ( filename ) );
	}
}

void Dvbv5::zap_parse_dvb_file ()
{
	uint16_t n = 1;
	struct dvb_file *dvb_file;
	struct dvb_entry *entry;

	dvb_file = dvb_read_file_format ( zap_file.c_str(), SYS_UNDEFINED, FILE_DVBV5 );

	if ( !dvb_file )
	{
		g_critical ( "%s:: Read file format failed.", __func__ );
		return;
	}

	zap->refTreeModel->clear ();

	for ( entry = dvb_file->first_entry; entry != NULL; entry = entry->next )
	{
		if ( entry->channel  )
		{
			Glib::ustring text = entry->channel;

			zap->append ( n, text, entry->service_id, 
				( ( entry->audio_pid ) ) ? entry->audio_pid[0] : 0, ( entry->video_pid ) ? entry->video_pid[0] : 0 );
		}

		if ( entry->vchannel )
		{
			Glib::ustring text = entry->channel;

			zap->append ( n, text, entry->service_id,
				( ( entry->audio_pid ) ) ? entry->audio_pid[0] : 0, ( entry->video_pid ) ? entry->video_pid[0] : 0 );
		}

		n++;
		if ( n >= UINT16_MAX ) break;
	}

	dvb_file_free ( dvb_file );
}

void Dvbv5::icon_press_zap ( Gtk::EntryIconPosition icon_position, const GdkEventButton* event )
{
	Glib::ustring filename = open_file ();

	if ( !filename.empty() )
	{
		zap_file = filename;
		zap->EntryConf->set_text ( Glib::path_get_basename ( filename ) );

		zap_parse_dvb_file ();
	}
}

void Dvbv5::create_signals ()
{
	scan->SpinButton[0]->signal_value_changed().connect ( sigc::mem_fun ( *this, &Dvbv5::update_device ) );
	scan->SpinButton[1]->signal_value_changed().connect ( sigc::mem_fun ( *this, &Dvbv5::update_device ) );

	scan->EntryInt->signal_icon_press().connect ( sigc::mem_fun ( *this, &Dvbv5::icon_press_inp ) );
	scan->EntryOut->signal_icon_press().connect ( sigc::mem_fun ( *this, &Dvbv5::icon_press_out ) );
	zap->EntryConf->signal_icon_press().connect ( sigc::mem_fun ( *this, &Dvbv5::icon_press_zap ) );

	zap->TreeView->signal_row_activated().connect ( sigc::mem_fun ( *this, &Dvbv5::treeview_row_act ) );

	sigc::slot<bool> tm_slot = sigc::bind ( sigc::mem_fun ( *this, &Dvbv5::timeout_update ), 0 );
	Glib::signal_timeout().connect ( tm_slot, 250 ); // Non-Stop
}

void Dvbv5::start ()
{
	verbose = ( debug ) ? 1 : 0;

	if ( ScanThread ) return;

	if ( !Glib::file_test ( input_file, Glib::FILE_TEST_EXISTS ) ) { message_dialog ( "File not found.", input_file ); return; }

	ScanThread = new std::thread ( [this] { thread_start (); } );
}

void Dvbv5::stop ()
{
	Status->set_text ( "Status: " );

	thread_stop ();

	dvb_zap_auto_free ();
}

void Dvbv5::mini ()
{
	if ( Notebook->is_visible () )
		Notebook->hide ();
	else
		Notebook->show ();

	resize ( 300, 100 );

}

void Dvbv5::dark ()
{
	auto settings = Gtk::Settings::get_default ();
	bool dark = settings->property_gtk_application_prefer_dark_theme ();
	settings->property_gtk_application_prefer_dark_theme () = !dark;
}

void Dvbv5::info ()
{
	Dialog.show();
}

void Dvbv5::quit ()
{
	StopInfo = true;
	stop ();

	if ( fd_info >= 0 ) dvb_close_device ( fd_info );

	close ();
}

