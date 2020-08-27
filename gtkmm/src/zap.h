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

#include <gtkmm.h>

typedef Glib::ustring ustr;

class Zap : public Gtk::Box
{
	public:
		Zap ();
		~Zap ();

		class ModelColumns : public Gtk::TreeModel::ColumnRecord
		{
			public:
				ModelColumns () { add(col_num); add(col_ch); add(col_sid); add(col_vpid); add(col_apid); }

				Gtk::TreeModelColumn<uint> col_num;
				Gtk::TreeModelColumn<ustr> col_ch;
				Gtk::TreeModelColumn<uint> col_sid;
				Gtk::TreeModelColumn<uint> col_vpid;
				Gtk::TreeModelColumn<uint> col_apid;
		};

		ModelColumns Columns;

		Gtk::TreeView *TreeView;
		Gtk::ScrolledWindow *ScrolledWindow;
		Glib::RefPtr<Gtk::ListStore> refTreeModel;

		void append ( uint16_t n, ustr ch, uint16_t s, uint16_t v, uint16_t a );

		Gtk::Entry *EntryConf;
		Gtk::ComboBoxText *ComboDemux;
};

#endif // ZAP_H
