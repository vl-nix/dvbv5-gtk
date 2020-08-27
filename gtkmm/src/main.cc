/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "dvbv5.h"

int main ( void )
{
	auto app = Gtk::Application::create ( "", Gio::APPLICATION_FLAGS_NONE );

	Dvbv5 dvbv5;

	return app->run ( dvbv5 );
}
