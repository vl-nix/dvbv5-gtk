/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef ZAP_SIGNALS_H
#define ZAP_SIGNALS_H

void zap_signals ( const OutDemux out_demux_n[], uint8_t n_elm, Dvbv5 *dvbv5 );
bool zap_signal_parse_dvb_file ( const char *file, Dvbv5 *dvbv5 );

#endif // ZAP_SIGNALS_H
