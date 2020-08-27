/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#ifndef DVBV5_H
#define DVBV5_H

#include <gtkmm.h>
#include <thread>
#include <mutex>

#include "zap.h"
#include "scan.h"
#include "level.h"

class Dvbv5 : public Gtk::Window
{
	public:
		Dvbv5 ();
		~Dvbv5 ();

	protected:
		Zap *zap;
		Scan *scan;
		Level *level;

		Gtk::Label *Device;
		Gtk::Label *Status;
		Gtk::Notebook *Notebook;
		Gtk::Button *Buttons[6];

		void dark  ();
		void mini  ();
		void info  ();
		void stop  ();
		void quit  ();
		void start ();

		void update_device  ();
		void create_signals ();
		void create_buttons ( Gtk::Box *HBox );
		void message_dialog ( Glib::ustring msg, Glib::ustring file );
		void treeview_row_act ( const Gtk::TreeModel::Path &path, Gtk::TreeViewColumn *column );

		Glib::ustring input_file, output_file, zap_file;
		Glib::ustring save_file ();
		Glib::ustring open_file ();
		void zap_parse_dvb_file ();
		void icon_press_inp ( Gtk::EntryIconPosition icon_position, const GdkEventButton* event );
		void icon_press_out ( Gtk::EntryIconPosition icon_position, const GdkEventButton* event );
		void icon_press_zap ( Gtk::EntryIconPosition icon_position, const GdkEventButton* event );

		int fd_info;
		bool StopInfo;
		bool timeout_update ( uint num );

		bool FinishThread;
		std::thread *ScanThread;
		mutable std::mutex Mutex;
		void thread_stop  ();
		void thread_start ();
		void thread_delete ();

		Gtk::AboutDialog Dialog;
		void about_dialog ();
		void about_dialog_response ( int res_id );

		bool debug;
};

#endif // DVBV5_H
