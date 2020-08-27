/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/


#ifndef SCAN_GST_H
#define SCAN_GST_H

#include <gst/gst.h>
#include <stdint.h>

typedef gboolean bool;

typedef struct _TcpServer TcpServer;

struct _TcpServer
{
	GstElement *pipeline;
	GstElement *file_src;
	GstElement *server_sink;
};

TcpServer * tcpserver_new ();
void tcpserver_destroy ( TcpServer *tcpserver );

void tcpserver_stop   ( TcpServer *tcpserver );
void tcpserver_start  ( TcpServer *tcpserver );
uint tcpserver_get ( TcpServer *tcpserver );
void tcpserver_set ( TcpServer *tcpserver, const char *host, uint port, const char *file );


typedef struct _Record Record;

struct _Record
{
	GstElement *pipeline;
	GstElement *file_src;
	GstElement *file_sink;
};

Record * record_new ();
void record_destroy ( Record *record );

void record_stop   ( Record *record );
void record_start  ( Record *record );
void record_set ( Record *record, const char *file, const char *file_rec );


typedef struct _Player Player;

struct _Player
{
	GstElement *playbin;
};

Player * player_new ( bool add_time );
void player_destroy ( Player *player );

void player_stop  ( Player *player );
void player_play  ( Player *player, const char *uri );


#endif
