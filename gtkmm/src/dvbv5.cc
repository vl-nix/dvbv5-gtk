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

#define NUM_BUTTONS 6

typedef void ( Dvbv5::*fp ) ();

Dvbv5::Dvbv5 ()
{
	std::string env = Glib::getenv ( "DVB_DEBUG", debug );
	if ( debug ) std::cout << "Set DVB_DEBUG" << std::endl;

	set_title ( "Dvbv5-Light-Gtkmm" );
	set_icon_name ( "display" );

	Notebook = manage ( new Gtk::Notebook () );
	Gtk::Box *VBox = manage ( new Gtk::Box ( Gtk::ORIENTATION_VERTICAL,   5 ) );
	Gtk::Box *HBox = manage ( new Gtk::Box ( Gtk::ORIENTATION_HORIZONTAL, 5 ) );

	Device = manage ( new Gtk::Label ( "Device"   ) );
	Status = manage ( new Gtk::Label ( "Status: ", Gtk::ALIGN_START, Gtk::ALIGN_CENTER, false ) );

	input_file  = "Initial";
	zap_file    = Glib::get_home_dir () + "/dvb_channel.conf";
	output_file = Glib::get_home_dir () + "/dvb_channel.conf";

	zap   = manage ( new Zap   () );
	scan  = manage ( new Scan  () );
	level = manage ( new Level () );
	ScanThread = nullptr;

	fd_info = -1;
	FinishThread = false;
	StopInfo = false;

	Gtk::Label *LabelScan = manage ( new Gtk::Label ( "Scan" ) );
	Gtk::Label *LabelZap  = manage ( new Gtk::Label ( "Zap"  ) );

	Notebook->append_page ( *scan, *LabelScan );
	Notebook->append_page ( *zap,  *LabelZap  );

	create_buttons ( HBox );
	create_signals ();

	VBox->pack_start ( *Device,  Gtk::PACK_SHRINK );
	VBox->pack_start ( *Notebook );
	VBox->pack_end   ( *HBox,    Gtk::PACK_SHRINK );
	VBox->pack_end   ( *level,   Gtk::PACK_SHRINK );
	VBox->pack_end   ( *Status,  Gtk::PACK_SHRINK );

	add ( *VBox );
	set_border_width ( 10 );
	show_all_children ();

	update_device ();
	about_dialog  ();
}

Dvbv5::~Dvbv5 ()
{
}

void Dvbv5::about_dialog ()
{
	Dialog.set_transient_for ( *this );

	Dialog.set_version ( VERSION );
	Dialog.set_logo_icon_name ( "display" );
	Dialog.set_program_name ( "Dvbv5-Light-Gtkmm" );
	Dialog.set_license_type ( Gtk::LICENSE_GPL_2_0 );
	Dialog.set_copyright ( "Copyright 2020 Dvbv5-Light-Gtkmm" );
	Dialog.set_comments  ( "Gtkmm interface to DVBv5 tool" );

	Dialog.set_website_label ( "Website" );
	Dialog.set_website ( "https://github.com/vl-nix/dvbv5-gtk" );

	std::vector<Glib::ustring> list_authors;
	list_authors.push_back ( "Mauro Carvalho Chehab" );
	list_authors.push_back ( "Stepan Perun" );
	list_authors.push_back ( "zampedro" );
	list_authors.push_back ( "Ro-Den" );
	Dialog.set_authors ( list_authors );

	Dialog.signal_response().connect ( sigc::mem_fun ( *this, &Dvbv5::about_dialog_response ) );
}

void Dvbv5::about_dialog_response ( int res_id )
{
	switch ( res_id )
	{
		case Gtk::RESPONSE_CLOSE:
		case Gtk::RESPONSE_CANCEL:
		case Gtk::RESPONSE_DELETE_EVENT:
		Dialog.hide();
			break;

		default:
			break;
	}
}

void Dvbv5::message_dialog ( Glib::ustring msg, Glib::ustring file )
{
	Gtk::MessageDialog dialog ( *this, msg );
	dialog.set_secondary_text ( file );

	dialog.run();
}

void Dvbv5::create_buttons ( Gtk::Box *HBox )
{
	const Glib::ustring b_n[] = { "⏵", "⏹", "🗕", "⏾", "🛈", "⏻" };
	fp funcs[] = { &Dvbv5::start, &Dvbv5::stop, &Dvbv5::mini, &Dvbv5::dark, &Dvbv5::info, &Dvbv5::quit };

	for ( uint8_t i = 0; i < NUM_BUTTONS; i++ )
	{
		Buttons[i] = manage ( new Gtk::Button ( b_n[i], false ) );
		Buttons[i]->signal_clicked().connect ( sigc::mem_fun ( *this, funcs[i] ) );
		HBox->pack_start ( *Buttons[i], Gtk::PACK_EXPAND_WIDGET );
	}
}

Glib::ustring Dvbv5::open_file ()
{
	Gtk::FileChooserDialog dialog ( "Open", Gtk::FILE_CHOOSER_ACTION_OPEN );
	dialog.set_transient_for (*this);

	dialog.add_button ( "_Cancel", Gtk::RESPONSE_CANCEL );
	dialog.add_button ( "_Open", Gtk::RESPONSE_OK );
	dialog.set_current_folder ( Glib::get_home_dir () );

	std::string filename;
	int result = dialog.run ();

	switch ( result )
	{
		case ( Gtk::RESPONSE_OK ):
		{
			filename = dialog.get_filename ();
			break;
		}

		case ( Gtk::RESPONSE_CANCEL ): break;

		default: break;
	}

	return filename;
}

Glib::ustring Dvbv5::save_file ()
{
	Gtk::FileChooserDialog dialog ( "Save", Gtk::FILE_CHOOSER_ACTION_SAVE );
	dialog.set_transient_for (*this);

	dialog.add_button ( "_Cancel", Gtk::RESPONSE_CANCEL );
	dialog.add_button ( "_Save", Gtk::RESPONSE_OK );

	dialog.set_current_folder ( Glib::get_home_dir () );
	dialog.set_current_name ( "dvb_channel.conf" );

	std::string filename;
	int result = dialog.run ();

	switch ( result )
	{
		case ( Gtk::RESPONSE_OK ):
		{
			filename = dialog.get_filename ();
			break;
		}

		case ( Gtk::RESPONSE_CANCEL ): break;

		default: break;
	}

	return filename;
}

