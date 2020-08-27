/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef LEVEL_H
#define LEVEL_H

#include <gtkmm.h>

class Level : public Gtk::Box
{
	public:
		Level ();
		~Level ();

		void level_set_sgl_snr ( float sgl, float snr, bool fe_lock );

	private:
		Gtk::Label *SglSnr;
		Gtk::ProgressBar *BarSgl;
		Gtk::ProgressBar *BarSnr;
};


#endif
