/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef SCAN_H
#define SCAN_H

#include <gtkmm.h>

class Scan : public Gtk::Grid
{
	public:
		Scan ();
		~Scan ();

		Gtk::SpinButton  *SpinButton[6];
		Gtk::CheckButton *CheckButton[4];
		Gtk::ComboBoxText *ComboLnb;
		Gtk::ComboBoxText *ComboLna;
		Gtk::Entry *EntryInt, *EntryOut;
};

#endif // SCAN_H
