/*
* Copyright 2022 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/gpl-2.0.html
*/

#include "dvb5-app.h"

int main ( int argc, char *argv[] )
{
	Dvb5App *app = dvb5_app_new ();

	int status = g_application_run ( G_APPLICATION ( app ), argc, argv );

	g_object_unref ( app );

	return status;
}
