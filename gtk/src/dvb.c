/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "dvb.h"

static u_int8_t _get_delsys ( struct dvb_v5_fe_parms *parms )
{
	u_int8_t sys = SYS_UNDEFINED;

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

static gpointer dvb_scan_thread ( Dvb *dvb_base )
{
	struct dvb_device *dvb = dvb_base->dvb_scan;
	struct dvb_v5_fe_parms *parms = dvb->fe_parms;
	struct dvb_file *dvb_file = NULL, *dvb_file_new = NULL;
	struct dvb_entry *entry;
	struct dvb_open_descriptor *dmx_fd;

	g_mutex_init ( &dvb_base->mutex );

	int count = 0, shift;
	u_int32_t freq = 0, sys = _get_delsys ( parms );
	enum dvb_sat_polarization pol;

	dvb_file = dvb_read_file_format ( dvb_base->input_file, sys, dvb_base->input_format );

	if ( !dvb_file )
	{
		dvb_dev_free ( dvb );
		dvb_base->dvb_scan  = NULL;
		dvb_base->demux_dev = NULL;

		g_mutex_clear ( &dvb_base->mutex );
		g_critical ( "%s:: Read file format failed.", __func__ );
		return NULL;
	}

	dmx_fd = dvb_dev_open ( dvb, dvb_base->demux_dev, O_RDWR );

	if ( !dmx_fd )
	{
		dvb_file_free ( dvb_file );
		dvb_dev_free ( dvb );
		dvb_base->dvb_scan  = NULL;
		dvb_base->demux_dev = NULL;

		g_mutex_clear ( &dvb_base->mutex );
		perror ( "opening demux failed" );
		return NULL;
	}

	for ( entry = dvb_file->first_entry; entry != NULL; entry = entry->next )
	{
		struct dvb_v5_descriptors *dvb_scan_handler = NULL;
		u_int32_t stream_id;

		if ( dvb_retrieve_entry_prop ( entry, DTV_FREQUENCY, &freq ) ) continue;

		shift = dvb_estimate_freq_shift ( parms );

		if ( dvb_retrieve_entry_prop ( entry, DTV_POLARIZATION, &pol ) ) pol = POLARIZATION_OFF;
		if ( dvb_retrieve_entry_prop ( entry, DTV_STREAM_ID, &stream_id ) ) stream_id = NO_STREAM_ID_FILTER;
		if ( !dvb_new_entry_is_needed ( dvb_file->first_entry, entry, freq, shift, pol, stream_id ) ) continue;

		count++;
		dvb_log ( "Scanning frequency #%d %d", count, freq );

		g_mutex_lock ( &dvb_base->mutex );
			dvb_base->freq_scan = freq;
		g_mutex_unlock ( &dvb_base->mutex );

		dvb_scan_handler = dvb_dev_scan ( dmx_fd, entry, &_check_frontend, NULL, dvb_base->other_nit, dvb_base->time_mult );

		g_mutex_lock ( &dvb_base->mutex );
			if ( dvb_scan_handler ) dvb_base->progs_scan += dvb_scan_handler->num_program;
			if ( dvb_base->thread_stop ) parms->abort = 1;
		g_mutex_unlock ( &dvb_base->mutex );

		if ( parms->abort )
		{
			dvb_scan_free_handler_table ( dvb_scan_handler );
			break;
		}

		if ( !dvb_scan_handler ) continue;

		dvb_store_channel ( &dvb_file_new, parms, dvb_scan_handler, dvb_base->get_detect, dvb_base->get_nit );

		if ( !dvb_base->new_freqs )
			dvb_add_scaned_transponders ( parms, dvb_scan_handler, dvb_file->first_entry, entry );

		dvb_scan_free_handler_table ( dvb_scan_handler );
	}

	if ( dvb_file_new ) dvb_write_file_format ( dvb_base->output_file, dvb_file_new, parms->current_sys, dvb_base->output_format );

	dvb_file_free ( dvb_file );
	if ( dvb_file_new ) dvb_file_free ( dvb_file_new );

	dvb_dev_close ( dmx_fd );

	g_mutex_lock ( &dvb_base->mutex );
		dvb_base->thread_stop = 1;
	g_mutex_unlock ( &dvb_base->mutex );

	g_mutex_clear ( &dvb_base->mutex );

	dvb_dev_free ( dvb );
	dvb_base->dvb_scan  = NULL;
	dvb_base->demux_dev = NULL;

	return NULL;
}

const char * dvb_scan ( Dvb *dvb )
{
	dvb->thread_stop = 0;

	dvb->dvb_scan = dvb_dev_alloc ();

	if ( !dvb->dvb_scan ) return "Allocates memory failed.";

	dvb_dev_set_log ( dvb->dvb_scan, 0, NULL );
	dvb_dev_find ( dvb->dvb_scan, NULL, NULL );

	struct dvb_dev_list *dvb_dev = dvb_dev_seek_by_adapter ( dvb->dvb_scan, dvb->adapter, dvb->demux, DVB_DEVICE_DEMUX );

	if ( !dvb_dev )
	{
		dvb_dev_free ( dvb->dvb_scan );
		dvb->dvb_scan = NULL;

		g_critical ( "%s:: Couldn't find demux device node.", __func__ );
		return "Couldn't find demux device.";
	}

	dvb->demux_dev = dvb_dev->sysname;
	g_message ( "%s:: demux_dev: %s ", __func__, dvb->demux_dev );

	dvb_dev = dvb_dev_seek_by_adapter ( dvb->dvb_scan, dvb->adapter, dvb->frontend, DVB_DEVICE_FRONTEND );

	if ( !dvb_dev )
	{
		dvb_dev_free ( dvb->dvb_scan );
		dvb->dvb_scan = NULL;

		g_critical ( "%s:: Couldn't find frontend device.", __func__ );
		return "Couldn't find frontend device.";
	}

	if ( !dvb_dev_open ( dvb->dvb_scan, dvb_dev->sysname, O_RDWR ) )
	{
		dvb_dev_free ( dvb->dvb_scan );
		dvb->dvb_scan = NULL;

		perror ( "Opening device failed" );
		return "Opening device failed.";
	}

	struct dvb_v5_fe_parms *parms = dvb->dvb_scan->fe_parms;

	if ( dvb->lnb >= 0 ) parms->lnb = dvb_sat_get_lnb ( dvb->lnb );
	if ( dvb->sat_num >= 0 ) parms->sat_number = dvb->sat_num;
	parms->diseqc_wait = dvb->diseqc_wait;
	parms->lna = dvb->lna;
	parms->freq_bpf = 0;

	dvb->thread = g_thread_new ( "scan-thread", (GThreadFunc)dvb_scan_thread, dvb );
	g_thread_unref ( dvb->thread );

	return NULL;
}

void dvb_scan_stop ( Dvb *dvb )
{
	if ( dvb->dvb_scan ) dvb->thread_stop = 1;
}

static u_int8_t dvb_zap_parse ( const char *file, const char *channel, u_int8_t frm, struct dvb_v5_fe_parms *parms, u_int16_t pids[], u_int16_t apids[] )
{
	struct dvb_file *dvb_file;
	struct dvb_entry *entry;

	u_int8_t i = 0, j = 0;
	u_int32_t sys = _get_delsys ( parms );

	dvb_file = dvb_read_file_format ( file, sys, frm );

	if ( !dvb_file )
	{
		g_critical ( "%s:: Read file format failed.", __func__ );
		return 0;
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
		u_int32_t f, freq = (u_int32_t)atoi ( channel );

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
		g_critical ( "%s:: channel %s | file %s | Can't find channel.", __func__, channel, file );

		dvb_file_free ( dvb_file );
		return 0;
	}

	if ( entry->lnb )
	{
		int lnb = dvb_sat_search_lnb ( entry->lnb );

		if ( lnb == -1 )
		{
			g_warning ( "%s:: Unknown LNB %s", __func__, entry->lnb );
			dvb_file_free ( dvb_file );
			return 0;
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
			if ( j >= MAX_AUDIO ) { printf ( "%s:: MAX_AUDIO %d: add apid stop \n", __func__, MAX_AUDIO ); break; }

			apids[j] = entry->audio_pid[j];
		}

		if ( j >= MAX_AUDIO ) pids[3] = MAX_AUDIO; else pids[3] = (u_int16_t)entry->audio_pid_len;
	}

	dvb_retrieve_entry_prop (entry, DTV_DELIVERY_SYSTEM, &sys );
	dvb_set_compat_delivery_system ( parms, sys );

	/* Copy data into parms */
	for ( i = 0; i < entry->n_props; i++ )
	{
		u_int32_t data = entry->props[i].u.data;

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

	return 1;
}

static u_int32_t dvb_zap_setup_frontend ( struct dvb_v5_fe_parms *parms )
{
	u_int32_t freq = 0;

	int rc = dvb_fe_retrieve_parm ( parms, DTV_FREQUENCY, &freq );

	if ( rc < 0 ) return 0;

	rc = dvb_fe_set_parms ( parms );

	if ( rc < 0 ) return 0;

	return freq;
}

static u_int8_t dvb_zap_set_pes_filter ( struct dvb_open_descriptor *fd, u_int16_t pid, dmx_pes_type_t type, dmx_output_t dmx, 
	u_int32_t buf_size, const char *type_name, u_int8_t debug )
{
	if ( debug ) g_message ( "  %s: DMX_SET_PES_FILTER %d (0x%04x)", type_name, pid, pid );

	if ( dvb_dev_dmx_set_pesfilter ( fd, pid, type, dmx, (int)buf_size ) < 0 ) return 0;

	return 1;
}

static u_int8_t dvb_zap_add_pat_pmt ( const char *demux_dev, u_int16_t sid, u_int8_t demux_descr, u_int32_t bsz, Dvb *dvb )
{
	int pmtpid = 0;

	struct dvb_open_descriptor *sid_fd;
	sid_fd = dvb_dev_open ( dvb->dvb_zap, demux_dev, O_RDWR );

	if ( sid_fd )
	{
		pmtpid = dvb_dev_dmx_get_pmt_pid ( sid_fd, sid );
		dvb_dev_close ( sid_fd );
	}
	else
		return 0;

	dvb->pat_fd = dvb_dev_open ( dvb->dvb_zap, demux_dev, O_RDWR );

	if ( dvb->pat_fd )
		dvb_zap_set_pes_filter ( dvb->pat_fd, 0, DMX_PES_OTHER, demux_descr, bsz, "PAT", dvb->debug );
	else
		return 0;

	dvb->pmt_fd = dvb_dev_open ( dvb->dvb_zap, demux_dev, O_RDWR );

	if ( dvb->pmt_fd )
		dvb_zap_set_pes_filter ( dvb->pmt_fd, (u_int16_t)pmtpid, DMX_PES_OTHER, demux_descr, bsz, "PMT", dvb->debug );
	else
		return 0;

	return 1;
}

static void dvb_zap_set_dmx ( Dvb *dvb )
{
	if ( dvb->descr_num == 5 ) return;

	if ( dvb->descr_num == 4 )
	{
		dvb->video_fd = dvb_dev_open ( dvb->dvb_zap, dvb->demux_dev, O_RDWR );

		if ( dvb->video_fd )
			dvb_zap_set_pes_filter ( dvb->video_fd, 0x2000, DMX_PES_OTHER, DMX_OUT_TS_TAP, 64 * 1024, "ALL", dvb->debug );
		else
			g_critical ( "%s:: failed opening %s", __func__, dvb->demux_dev );

		return;
	}

	u_int32_t bsz = ( dvb->descr_num == DMX_OUT_TS_TAP || dvb->descr_num == 4 ) ? 64 * 1024 : 0;

	if ( dvb->pids[4] ) 
		dvb_zap_add_pat_pmt ( dvb->demux_dev, dvb->pids[0], dvb->descr_num, bsz, dvb );
	// else /* uncomment for SID = 0 */
		// dvbv5_add_pat_pmt ( dvb->demux_dev, 0, dvb->descr_num, bsz, dvb );

	if ( dvb->pids[5] )
	{
		dvb->video_fd = dvb_dev_open ( dvb->dvb_zap, dvb->demux_dev, O_RDWR );

		if ( dvb->video_fd )
			dvb_zap_set_pes_filter ( dvb->video_fd, dvb->pids[1], DMX_PES_VIDEO, dvb->descr_num, bsz, "VIDEO", dvb->debug );
		else
			g_critical ( "%s:: VIDEO: failed opening %s", __func__, dvb->demux_dev );
	}

	if ( dvb->pids[3] )
	{
		u_int8_t j = 0; for ( j = 0; j < dvb->pids[3]; j++ )
		{
			dvb->audio_fds[j] = dvb_dev_open ( dvb->dvb_zap, dvb->demux_dev, O_RDWR );

			if ( dvb->audio_fds[j] )
				dvb_zap_set_pes_filter ( dvb->audio_fds[j], dvb->apids[j], ( j == 0 ) ? DMX_PES_AUDIO : DMX_PES_OTHER, dvb->descr_num, bsz, "AUDIO", dvb->debug );
			else
				g_critical ( "%s:: AUDIO: failed opening %s", __func__, dvb->demux_dev );
		}
	}
}

static gboolean dvb_zap_check_fe_lock ( Dvb *dvb )
{
	if ( !dvb || !dvb->dvb_zap ) return FALSE;

	static u_int32_t time = 1;
	struct dvb_v5_fe_parms *parms = dvb->dvb_zap->fe_parms;

	dvb_fe_get_stats ( parms ); // error is ignored

	fe_status_t status;
	dvb_fe_retrieve_stats ( parms, DTV_STATUS, &status );

	gboolean fe_lock = FALSE;
	if ( status & FE_HAS_LOCK ) fe_lock = TRUE;

	if ( fe_lock ) dvb_zap_set_dmx ( dvb );
	if ( fe_lock ) { time = 1; return FALSE; }

	if ( dvb->debug ) g_message ( "%s: FE_HAS_LOCK failed ( %d milisecond ) ", __func__, time * 250 );
	time++;

	return TRUE;
}

void dvb_zap_stop ( Dvb *dvb )
{
	if ( dvb->dvb_zap )
	{
		if ( dvb->pat_fd ) dvb_dev_close ( dvb->pat_fd );
		if ( dvb->pmt_fd ) dvb_dev_close ( dvb->pmt_fd );
		if ( dvb->video_fd ) dvb_dev_close ( dvb->video_fd );

		dvb->pat_fd = NULL;
		dvb->pmt_fd = NULL;
		dvb->video_fd = NULL;

		u_int8_t j = 0; for ( j = 0; j < dvb->pids[3]; j++ )
		{
			if ( dvb->audio_fds[j] ) dvb_dev_close ( dvb->audio_fds[j] );
			dvb->audio_fds[j] = NULL;
		}

		for ( j = 0; j < 6; j++ ) dvb->pids[j] = 0;

		dvb_dev_free ( dvb->dvb_zap );
		dvb->dvb_zap = NULL;
		dvb->demux_dev = NULL;
	}
}

const char * dvb_zap ( const char *channel, u_int8_t num, Dvb *dvb )
{
	dvb->dvb_zap = dvb_dev_alloc ();

	if ( !dvb->dvb_zap ) return "Allocates memory failed.";

	dvb_dev_set_log ( dvb->dvb_zap, 0, NULL );
	dvb_dev_find ( dvb->dvb_zap, NULL, NULL );
	struct dvb_v5_fe_parms *parms = dvb->dvb_zap->fe_parms;

	struct dvb_dev_list *dvb_dev = dvb_dev_seek_by_adapter ( dvb->dvb_zap, dvb->adapter, dvb->demux, DVB_DEVICE_DEMUX );

	if ( !dvb_dev )
	{
		dvb_dev_free ( dvb->dvb_zap );
		dvb->dvb_zap = NULL;

		g_critical ( "%s: Couldn't find demux device node.", __func__ );
		return "Couldn't find demux device.";
	}

	dvb->demux_dev = dvb_dev->sysname;

	dvb_dev = dvb_dev_seek_by_adapter ( dvb->dvb_zap, dvb->adapter, dvb->demux, DVB_DEVICE_DVR );

	if ( !dvb_dev )
	{
		dvb_dev_free ( dvb->dvb_zap );
		dvb->dvb_zap = NULL;

		g_critical ( "%s: Couldn't find dvr device node.", __func__ );
		return "Couldn't find dvr device.";
	}

	dvb_dev = dvb_dev_seek_by_adapter ( dvb->dvb_zap, dvb->adapter, dvb->frontend, DVB_DEVICE_FRONTEND );

	if ( !dvb_dev )
	{
		dvb_dev_free ( dvb->dvb_zap );
		dvb->dvb_zap = NULL;

		g_critical ( "%s: Couldn't find frontend device node.", __func__ );
		return "Couldn't find frontend device.";
	}

	if ( !dvb_dev_open ( dvb->dvb_zap, dvb_dev->sysname, O_RDWR ) )
	{
		dvb_dev_free ( dvb->dvb_zap );
		dvb->dvb_zap = NULL;

		perror ( "Opening device failed" );
		return "Opening device failed.";
	}

	// pids[6];  0 - sid, 1 - vpid, 2 - apid, 3 - apid_len, 4 - sid found, 5 - vpid found
	u_int8_t j = 0;
	for ( j = 0; j < 6; j++ ) dvb->pids[j] = 0;
	for ( j = 0; j < MAX_AUDIO; j++ ) { dvb->apids[j] = 0; dvb->audio_fds[j] = NULL; }

	parms->diseqc_wait = 0;
	parms->freq_bpf = 0;
	parms->lna = -1;

	if ( !dvb_zap_parse ( dvb->zap_file, channel, FILE_DVBV5, parms, dvb->pids, dvb->apids ) )
	{
		dvb_dev_free ( dvb->dvb_zap );
		dvb->dvb_zap = NULL;

		g_critical ( "%s:: Zap parse failed.", __func__ );
		return "Zap parse failed.";
	}

	u_int32_t freq = dvb_zap_setup_frontend ( parms );

	if ( freq )
	{
		dvb->descr_num = num;
		dvb->freq_scan = freq;

		if ( dvb->debug ) g_message ( "%s:: Zap Ok.", __func__ );
	}
	else
	{
		dvb_dev_free ( dvb->dvb_zap );
		dvb->dvb_zap = NULL;

		g_warning ( "%s:: Zap failed.", __func__ );
		return "Zap failed.";
	}

	g_timeout_add ( 250, (GSourceFunc)dvb_zap_check_fe_lock, dvb );

	return NULL;
}

static int _frontend_stats ( struct dvb_v5_fe_parms *parms )
{
	char buf[512], *p;
	int rc, i, len, show;

	rc = dvb_fe_get_stats ( parms );
	if ( rc ) return -1;

	p = buf;
	len = sizeof(buf);
	dvb_fe_snprintf_stat ( parms, DTV_STATUS, NULL, 0, &p, &len, &show );

	for ( i = 0; i < MAX_DTV_STATS; i++ )
	{
		show = 1;

		dvb_fe_snprintf_stat ( parms, DTV_QUALITY, "Quality", i, &p, &len, &show );
		dvb_fe_snprintf_stat ( parms, DTV_STAT_SIGNAL_STRENGTH, "Signal", i, &p, &len, &show );
		dvb_fe_snprintf_stat ( parms, DTV_STAT_CNR, "C/N", i, &p, &len, &show );
		dvb_fe_snprintf_stat ( parms, DTV_STAT_ERROR_BLOCK_COUNT, "UCB", i,  &p, &len, &show );
		dvb_fe_snprintf_stat ( parms, DTV_BER, "postBER", i,  &p, &len, &show );
		dvb_fe_snprintf_stat ( parms, DTV_PRE_BER, "preBER", i,  &p, &len, &show );
		dvb_fe_snprintf_stat ( parms, DTV_PER, "PER", i,  &p, &len, &show );

		if ( p != buf )
		{
			if ( i )
				printf ( "\t%s\n", buf );
			else
				printf ( "%s\n", buf );

			p = buf;
			len = sizeof(buf);
		}
	}

	return 0;
}

static char * dvb_fe_get_stats_layer_str ( struct dvb_v5_fe_parms *parms, u_int8_t cmd, const char *name, u_int8_t layer, u_int8_t debug )
{
	char *ret = NULL;

	struct dtv_stats *stat = dvb_fe_retrieve_stats_layer ( parms, cmd, layer );

	if ( !stat || stat->scale == FE_SCALE_NOT_AVAILABLE ) return ret;

	const char *global = ( layer ) ? "Layer" : "Global";

	switch ( stat->scale )
	{
		case FE_SCALE_DECIBEL:
			if ( debug ) printf ( "%s:: %s ( Decibel %lld ) \n", global, name, stat->svalue );
			if ( cmd == DTV_STAT_SIGNAL_STRENGTH )
				ret = g_strdup_printf ( "%s %.2fdBm", name, (double)stat->svalue / 1000. );
			else
				ret = g_strdup_printf ( "%s %.2fdB", name, (double)stat->svalue / 1000. );
			break;
		case FE_SCALE_RELATIVE:
			if ( debug ) printf ( "%s:: %s ( Relative %llu ) \n", global, name, stat->uvalue );
			ret = g_strdup_printf ( "%s %3.0f%%", name, ( 100 * (double)stat->uvalue ) / 65535. );
			break;
		default:
			break;
	}

	return ret;
}

static double dvb_fe_get_stats_layer_digits ( struct dvb_v5_fe_parms *parms, u_int8_t cmd, const char *name, u_int8_t layer, u_int8_t debug )
{
	double ret = 0;

	struct dtv_stats *stat = dvb_fe_retrieve_stats_layer ( parms, cmd, layer );

	if ( !stat || stat->scale == FE_SCALE_NOT_AVAILABLE ) return ret;

	const char *global = ( layer ) ? "Layer" : "Global";

	switch ( stat->scale )
	{
		case FE_SCALE_DECIBEL:
			if ( debug ) printf ( "%s:: %s ( Decibel %lld ) \n", global, name, stat->svalue );
			if ( cmd == DTV_STAT_SIGNAL_STRENGTH )
				ret = (double)stat->svalue / 1000.;
			else
				ret = (double)stat->svalue / 1000.;
			break;
		case FE_SCALE_RELATIVE:
			if ( debug ) printf ( "%s:: %s ( Relative %llu ) \n", global, name, stat->uvalue );
			ret = ( 100 * (double)stat->uvalue ) / 65535.;
			break;
		default:
			break;
	}

	return ret;
}

static u_int8_t dvb_fe_get_m_qual ( struct dvb_v5_fe_parms *parms, u_int16_t vpid, uint bitrate, u_int8_t debug )
{
	u_int8_t qual = 0;
	enum fecap_scale_params scale;
	float ber = dvb_fe_retrieve_ber ( parms, 0, &scale );

	u_int32_t allow_bits_error_good = ( bitrate ) ? bitrate / 10 : 200, allow_bits_error_ok = ( bitrate ) ? bitrate : 2000;
	if ( !vpid && !bitrate ) { allow_bits_error_good = 20; allow_bits_error_ok = 200; } // Radio

	if ( debug ) g_message ( "%s:: ber = %.0f | abe_good = %d | abe_ok = %d ", __func__, ber, allow_bits_error_good, allow_bits_error_ok );

	if ( ber >  allow_bits_error_ok   ) qual = 1; // Poor ( BER > 1e−3: 1 error per 1000  bits )
	if ( ber <= allow_bits_error_ok   ) qual = 2; // 1e-4 > Ok < 1e-3 ( allow-bits-error )
	if ( ber <= allow_bits_error_good ) qual = 3; // Good ( BER < 1e-4: 1 error per 10000 bits )

	return qual;
}

u_int8_t dvb_fe_get_is_satellite ( u_int8_t delsys )
{
	switch ( delsys )
	{
		case SYS_DVBS:
		case SYS_DVBS2:
		case SYS_TURBO:
		case SYS_ISDBS:
			return 1;

		default:
			return 0;
	}
}

DvbStat dvb_fe_stat_get ( uint bitrate, Dvb *dvb )
{
	DvbStat dvbstat = {};

	struct dvb_v5_fe_parms *parms = dvb->dvb_fe->fe_parms;

	int rc = dvb_fe_get_stats ( parms );

	if ( rc )
	{
		g_warning ( "%s:: failed.", __func__ );

		return dvbstat;
	}

	u_int32_t freq = 0, qual = 0;
	gboolean fe_lock = FALSE;

	fe_status_t status;
	dvb_fe_retrieve_stats ( parms, DTV_STATUS,  &status );
	dvb_fe_retrieve_stats ( parms, DTV_QUALITY,   &qual );
	dvb_fe_retrieve_parm  ( parms, DTV_FREQUENCY, &freq );

	if ( status & FE_HAS_LOCK ) fe_lock = TRUE;
	dvb->fe_lock = fe_lock;

	if ( qual == 0 && fe_lock && dvb->dvb_zap ) qual = dvb_fe_get_m_qual ( parms, dvb->pids[1], bitrate, dvb->debug );

	u_int8_t layer = 0;
	char *sgl_gs = NULL, *snr_gs = NULL;
	double sgl_gd = 0, snr_gd = 0;

	sgl_gs = dvb_fe_get_stats_layer_str ( parms, DTV_STAT_SIGNAL_STRENGTH, "Signal", layer, dvb->debug );
	snr_gs = dvb_fe_get_stats_layer_str ( parms, DTV_STAT_CNR, "Snr", layer, dvb->debug );

	if ( sgl_gs && g_strrstr ( sgl_gs, "dB" ) ) layer = 1;

	sgl_gd = dvb_fe_get_stats_layer_digits ( parms, DTV_STAT_SIGNAL_STRENGTH, "Signal", layer, dvb->debug );
	snr_gd = dvb_fe_get_stats_layer_digits ( parms, DTV_STAT_CNR, "Snr", layer, dvb->debug );

	dvbstat.fe_lock = fe_lock;
	dvbstat.qual = qual;
	dvbstat.freq = ( dvb->freq_scan ) ? dvb->freq_scan : freq;
	dvbstat.sgl = sgl_gd;
	dvbstat.snr = snr_gd;
	dvbstat.sgl_str = sgl_gs;
	dvbstat.snr_str = snr_gs;

	dvbstat.freq /= ( dvb_fe_is_satellite ( parms->current_sys ) ) ? 1000 : 1000000;

/*
	u_int32_t sgl = 0, snr = 0;
	dvb_fe_retrieve_stats ( parms, DTV_STAT_CNR, &snr );
	dvb_fe_retrieve_stats ( parms, DTV_STAT_SIGNAL_STRENGTH, &sgl );
*/

	if ( dvb->debug ) _frontend_stats ( parms );

	return dvbstat;
}

const char * dvb_info ( Dvb *dvb )
{
	dvb->dvb_fe = dvb_dev_alloc ();

	if ( !dvb->dvb_fe ) return "Allocates memory failed.";

	dvb_dev_set_log ( dvb->dvb_fe, 0, NULL );
	dvb_dev_find ( dvb->dvb_fe, NULL, NULL );

	struct dvb_dev_list *dvb_dev_fe = dvb_dev_seek_by_adapter ( dvb->dvb_fe, dvb->adapter, dvb->frontend, DVB_DEVICE_FRONTEND );

	if ( !dvb_dev_fe )
	{
		g_critical ( "%s: Couldn't find demux device.", __func__ );
		return "Couldn't find demux device.";
	}

	if ( !dvb_dev_open ( dvb->dvb_fe, dvb_dev_fe->sysname, O_RDONLY ) )
	{
		g_critical ( "%s: Opening device failed.", __func__ );
		return "Opening device failed.";
	}

	return NULL;
}

char * dvb_info_get_dvb_name ( Dvb *dvb )
{
	char *ret = NULL;

	if ( dvb->dvb_fe ) { dvb_dev_free ( dvb->dvb_fe ); dvb->dvb_fe = NULL; }

	const char *error = dvb_info ( dvb );
	
	if ( error )
	{
		if ( dvb->dvb_fe ) { dvb_dev_free ( dvb->dvb_fe ); dvb->dvb_fe = NULL; }

		return g_strdup ( "Undefined" ); 
	}

	struct dvb_v5_fe_parms *parms = dvb->dvb_fe->fe_parms;

	ret = g_strdup ( parms->info.name );

	if ( dvb->dvb_fe ) { dvb_dev_free ( dvb->dvb_fe ); dvb->dvb_fe = NULL; }

	return ret;
}

void dvb_info_stop ( Dvb *dvb )
{
	if ( dvb->dvb_scan || !dvb->dvb_fe ) return;

	dvb_dev_free ( dvb->dvb_fe );
	dvb->dvb_fe = NULL;
}

void dvb_set_zero ( Dvb *dvb )
{
	dvb->thread_stop = 0;
	dvb->progs_scan  = 0;
	if ( !dvb->dvb_zap ) dvb->freq_scan = 0;
}

static void dvb_init ( Dvb *dvb )
{
	dvb->adapter   = 0;
	dvb->frontend  = 0;
	dvb->demux     = 0;
	dvb->time_mult = 2;

	dvb->new_freqs  = 0;
	dvb->get_detect = 0;
	dvb->get_nit    = 0;
	dvb->other_nit  = 0;

	dvb->lna = -1;
	dvb->lnb = -1;
	dvb->sat_num = -1;
	dvb->diseqc_wait = 0;

	dvb->input_file   = g_strdup ( "Initial" );
	dvb->output_file  = g_strconcat ( g_get_home_dir (), "/dvb_channel.conf", NULL );
	dvb->zap_file     = g_strdup ( dvb->output_file );

	dvb->input_format  = FILE_DVBV5;
	dvb->output_format = FILE_DVBV5;

	dvb->debug = ( g_getenv ( "DVB_DEBUG" ) ) ? 1 : 0;
}

Dvb * dvb_new ( void )
{
	Dvb *dvb = g_new0 ( Dvb, 1 );

	dvb_init ( dvb );

	return dvb;
}

