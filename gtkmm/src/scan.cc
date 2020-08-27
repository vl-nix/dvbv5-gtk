/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "scan.h"

static const char *lnb_n[][2] =
{
	{ "EXT",   "EXTENDED" },
	{ "UNV",   "UNIVERSAL" },
	{ "DBS",   "DBS" },
	{ "STD",   "STANDARD" },
	{ "L1070", "L10700" },
	{ "L1075", "L10750" },
	{ "L1130", "L11300" },
	{ "ENH",   "ENHANCED" },
	{ "QPH",   "QPH031" },
	{ "CBD",   "C-BAND" },
	{ "CMT",   "C-MULT" },
	{ "DSH",   "DISHPRO" },
	{ "BSJ",   "110BS" },
	{ "STBRS", "STACKED-BRASILSAT" },
	{ "OIBRS", "OI-BRASILSAT" },
	{ "AMZ3",  "AMAZONAS" },
	{ "AMZ2",  "AMAZONAS" },
	{ "GVBRS", "GVT-BRASILSAT" }
};

Scan::Scan ()
{
	set_row_spacing ( 5 );
	set_column_spacing ( 10 );

	set_margin_top ( 10 );
	set_margin_end ( 10 );
	set_margin_start ( 10 );
	set_margin_bottom ( 10 );

	struct Data { const char *text; uint8_t value; const char *text_2; uint8_t value_2; } data_n[] =
	{
		{ "Adapter",    0, "Frontend",     0 },
		{ "Demux",   	0, "Timeout",      2 },
		{ "Freqs-only", 0, "Get frontend", 0 },
		{ "Nit",   	0, "Other nit",    0 },
		{ "LNB", 	0, "DISEqC",   	   0 },
		{ "LNA",   	0, "Wait DISEqC",  0 }
	};

	uint8_t d = 0, j = 0, spin_num = 0, toggle_num = 0;

	for ( d = 0; d < G_N_ELEMENTS ( data_n ); d++ )
	{
		Gtk::Label *label = manage ( new Gtk::Label ( data_n[d].text, Gtk::ALIGN_START, Gtk::ALIGN_CENTER, false ) );
		attach ( *label, 0, d, 1, 1 );

		label = manage ( new Gtk::Label ( data_n[d].text_2, Gtk::ALIGN_START, Gtk::ALIGN_CENTER, false ) );
		attach ( *label, 2, d, 1, 1 );

		if ( d == 0 || d == 1 )
		{
			uint8_t i = spin_num++;
			SpinButton[i] = manage ( new Gtk::SpinButton () );
			SpinButton[i]->set_range ( 0, 16 );
			SpinButton[i]->set_increments ( 1, 5 );
			SpinButton[i]->set_value ( data_n[d].value );
			attach ( *SpinButton[i], 1, d, 1, 1 );
		}
		else if ( d == 2 || d == 3 )
		{
			uint8_t i = toggle_num++;
			CheckButton[i] = manage ( new Gtk::CheckButton () );
			attach ( *CheckButton[i], 1, d, 1, 1 );
		}
		else
		{
			if ( d == 4 )
			{
				ComboLnb = manage ( new Gtk::ComboBoxText () );
				ComboLnb->append ( "NONE", "None" );
				for ( j = 0; j < G_N_ELEMENTS ( lnb_n ); j++ ) ComboLnb->append ( lnb_n[j][1], lnb_n[j][0] );
				ComboLnb->set_active ( 0 );
				attach ( *ComboLnb, 1, d, 1, 1 );
			}
			else
			{
				ComboLna = manage ( new Gtk::ComboBoxText () );
				ComboLna->append ( "ON",   "On"   );
				ComboLna->append ( "OFF",  "Off"  );
				ComboLna->append ( "AUTO", "Auto" );
				ComboLna->set_active ( 2 );
				attach ( *ComboLna, 1, d, 1, 1 );
			}
		}

		if ( d == 0 || d == 1 )
		{
			uint8_t i = spin_num++;
			SpinButton[i] = manage ( new Gtk::SpinButton () );
			SpinButton[i]->set_range ( 0, ( d == 0 ) ? 16 : 255 );
			SpinButton[i]->set_increments ( 1, 5 );
			SpinButton[i]->set_value ( data_n[d].value_2 );
			attach ( *SpinButton[i], 3, d, 1, 1 );
		}
		else if ( d == 2 || d == 3 )
		{
			uint8_t i = toggle_num++;
			CheckButton[i] = manage ( new Gtk::CheckButton () );
			attach ( *CheckButton[i], 3, d, 1, 1 );
		}
		else
		{
			uint8_t i = spin_num++;
			SpinButton[i] = manage ( new Gtk::SpinButton () );
			SpinButton[i]->set_range ( ( d == 4 ) ? -1 : 0, ( d == 4 ) ? 48 : 255 );
			SpinButton[i]->set_increments ( 1, 5 );
			SpinButton[i]->set_value ( ( d == 4 ) ? -1 : 0 );
			attach ( *SpinButton[i], 3, d, 1, 1 );
		}
	}

	EntryInt = manage ( new Gtk::Entry () );
	EntryInt->set_editable ( false );
	EntryInt->set_text ( "Initial file" );
	EntryInt->set_icon_from_icon_name ( "folder", Gtk::ENTRY_ICON_SECONDARY );
	attach ( *EntryInt, 0, d, 2, 1 );

	EntryOut = manage ( new Gtk::Entry () );
	EntryOut->set_editable ( false );
	EntryOut->set_text ( "dvb_channel.conf" );
	EntryOut->set_icon_from_icon_name ( "folder", Gtk::ENTRY_ICON_SECONDARY );
	attach ( *EntryOut, 2, d, 2, 1 );
}

Scan::~Scan ()
{
}
