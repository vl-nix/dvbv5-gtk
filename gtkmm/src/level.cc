/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "level.h"

Level::Level ()
{
	set_orientation ( Gtk::ORIENTATION_VERTICAL );
	set_spacing ( 3 );

	SglSnr = manage ( new Gtk::Label ( "Signal  ◉  Snr" ) );
	BarSgl = manage ( new Gtk::ProgressBar () );
	BarSnr = manage ( new Gtk::ProgressBar () );

	pack_start ( *SglSnr, Gtk::PACK_EXPAND_WIDGET );
	pack_start ( *BarSgl, Gtk::PACK_EXPAND_WIDGET );
	pack_start ( *BarSnr, Gtk::PACK_EXPAND_WIDGET );
}

Level::~Level ()
{
}

void Level::level_set_sgl_snr ( float sgl, float snr, bool fe_lock )
{
	BarSgl->set_fraction ( sgl / 100 );
	BarSnr->set_fraction ( snr / 100 );

	Glib::ustring color = "#00ff00";

	if ( !fe_lock ) color = "#ff0000";
	if ( sgl == 0 && snr == 0 ) color = "#bfbfbf";

	Glib::ustring markup = Glib::ustring::compose ( "Signal %1%%<span foreground=\"%2\">  ◉  </span>Snr %3%%", sgl, color, snr );

	SglSnr->set_markup ( markup );
}
