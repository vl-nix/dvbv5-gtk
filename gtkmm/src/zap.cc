/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "zap.h"

#define NUM_DMX 6

Zap::Zap ()
{
	set_orientation ( Gtk::ORIENTATION_VERTICAL );

	TreeView = manage ( new Gtk::TreeView () );
	ScrolledWindow = manage ( new Gtk::ScrolledWindow () );
	ScrolledWindow->set_policy ( Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC );

	refTreeModel = Gtk::ListStore::create ( Columns );
	TreeView->set_model ( refTreeModel );

	ScrolledWindow->add ( *TreeView );
	pack_start ( *ScrolledWindow );

	TreeView->append_column ( "Num", Columns.col_num );
	TreeView->append_column ( "Channel", Columns.col_ch );
	TreeView->append_column ( "Sid",  Columns.col_sid  );
	TreeView->append_column ( "Vpid", Columns.col_vpid );
	TreeView->append_column ( "Apid", Columns.col_apid );

	Gtk::Box *HBox = manage ( new Gtk::Box ( Gtk::ORIENTATION_HORIZONTAL, 5 ) );

	EntryConf = manage ( new Gtk::Entry () );
	EntryConf->set_editable ( false );
	EntryConf->set_text ( "dvb_channel.conf" );
	EntryConf->set_icon_from_icon_name ( "folder", Gtk::ENTRY_ICON_SECONDARY );

	const Glib::ustring d_n[NUM_DMX] = { "DMX_OUT_DECODER", "DMX_OUT_TAP", "DMX_OUT_TS_TAP", "DMX_OUT_TSDEMUX_TAP", "DMX_OUT_ALL_PIDS", "DMX_OUT_OFF" };

	ComboDemux = manage ( new Gtk::ComboBoxText () );
	for ( uint8_t i = 0; i < NUM_DMX; i++ ) ComboDemux->append ( d_n[i], d_n[i] );
	ComboDemux->set_active ( 2 );

	HBox->pack_start ( *EntryConf  );
	HBox->pack_start ( *ComboDemux );
	pack_start ( *HBox, Gtk::PACK_SHRINK );
}

Zap::~Zap ()
{
}

void Zap::append ( uint16_t n, ustr ch, uint16_t s, uint16_t v, uint16_t a )
{
	Gtk::TreeModel::Row row = *(refTreeModel->append());
	row[Columns.col_num]  = n;
	row[Columns.col_ch]   = ch;
	row[Columns.col_sid]  = s;
	row[Columns.col_vpid] = v;
	row[Columns.col_apid] = a;
}
