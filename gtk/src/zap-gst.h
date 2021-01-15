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

#ifndef LIGHT

#include <gst/gst.h>
#include <stdint.h>

typedef struct _TcpServer TcpServer;

struct _TcpServer
{
	GstElement *pipeline;
	GstElement *file_src;
	GstElement *server_sink;
};

TcpServer * tcpserver_new ( void );
void tcpserver_destroy ( TcpServer * );

void tcpserver_stop   ( TcpServer * );
void tcpserver_start  ( TcpServer * );
void tcpserver_set ( TcpServer *, const char *, u_int16_t, const char * );


typedef struct _Record Record;

struct _Record
{
	GstElement *pipeline;
	GstElement *file_src;
	GstElement *file_sink;
};

Record * record_new ( void );
void record_destroy ( Record * );

void record_stop   ( Record * );
void record_start  ( Record * );
void record_set ( Record *, const char *, const char * );


typedef struct _Player Player;

struct _Player
{
	GstElement *playbin;
};

Player * player_new ( void );
void player_destroy ( Player * );

void player_stop  ( Player * );
void player_play  ( Player *, const char * );


typedef struct _PlayerPipe PlayerPipe;

struct _PlayerPipe
{
	GstElement *pipeline;
	GstElement *fd_src;
	GstElement *vqueue2;
	GstElement *aqueue2;
};

PlayerPipe * player_pipe_new ( u_int16_t );
void player_pipe_destroy ( PlayerPipe * );

void player_pipe_stop  ( PlayerPipe * );
void player_pipe_play  ( PlayerPipe *, int );

#endif

#endif
