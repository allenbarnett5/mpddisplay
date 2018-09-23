/*
 * Encapsulate the interaction with MPD. Because this is a network
 * application, there is a lot which can go wrong.
 *
 * In this version, we try to use libmpdclient since it provides more
 * functionality (namely writing to the server) than I want to code by
 * hand.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "glib.h"

#include "mpd/client.h"

#include "log_intf.h"
#include "mpd_intf.h"

/*!
 * This is the data extracted from the MPD command. changed is updated
 * with MPD_CHANGED flags to note differences from the current values.
 */
struct MPD_CURRENT {
  //! Bitmap of changed values.
  int changed;
  //! Play status.
  enum MPD_PLAY_STATUS play_status;
  //! The artist (UTF-8)
  GString* artist;
  //! The album (UTF-8)
  GString* album;
  //! The title (UTF-8)
  GString* title;
  //! Elapsed time in seconds.
  unsigned int elapsed_time;
  //! Total track time in seconds.
  unsigned int total_time;
};
/*!
 * The details of the MPD connection.
 */
struct MPD_PRIVATE {
  //! Our connection to MPD.
  struct mpd_connection* connection;
  //! The socket file descriptor.
  int fd;
  //! Host string.
  GString* host;
  //! Port number (libmpdclient doesn't accept "service")
  int port;
  //! Our logging object.
  struct LOG_HANDLE logger;
  //! The results of the last poll of the server.
  struct MPD_CURRENT current;
};

static void mpd_current_init ( struct MPD_CURRENT* current )
{
  current->changed = 0;
  current->play_status = MPD_PLAY_STATUS_NOSONG;
  current->artist = g_string_sized_new( 256 );
  current->album = g_string_sized_new( 256 );
  current->title = g_string_sized_new( 256 );
  current->elapsed_time = 0;
  current->total_time = 0;
}

static void mpd_current_free ( struct MPD_CURRENT* current )
{
  g_string_free( current->artist, TRUE );
  g_string_free( current->album, TRUE );
  g_string_free( current->title, TRUE );
}

static void get_song_string ( struct mpd_song* song,
			      enum mpd_tag_type type,
			      GString* result )
{
  unsigned int i = 0;
  const char* value; // This is UTF-8 according to the docs.
  while ( ( value = mpd_song_get_tag( song, type, i ) ) != NULL ) {
    if ( i > 0 ) {
      g_string_append( result, " - " );
    }

    g_string_append( result, value );

    ++i;
  }
}

static int mpd_get_current ( struct mpd_connection* connection,
			     struct LOG_HANDLE logger,
			     struct MPD_CURRENT* previous )
{
  // So, this is the nub of it. Send a command to MPD and await its
  // response.
  struct MPD_CURRENT current;
  mpd_current_init( &current );
  if ( ! mpd_command_list_begin( connection, true ) ) {
    log_message_error( logger, "could not begin command list" );
  }
  if ( ! mpd_send_status( connection ) ) {
    log_message_error( logger, "could not send \"status\"" );
  }
  if ( ! mpd_send_current_song( connection ) ) {
    log_message_error( logger, "could not send \"current song\"" );
  }
  if ( ! mpd_command_list_end( connection ) ) {
    log_message_error( logger, "could not end command list" );
  }
  
  struct mpd_status* status = mpd_recv_status( connection );

  if ( status == NULL ) {
    log_message_error( logger, "error retrieving status" );
    return -1;
  }

  if ( mpd_status_get_error( status ) != NULL ) {
    log_message_error( logger, "error in status: %s",
		       mpd_status_get_error( status ) );
    mpd_status_free( status );
    return -1;
  }

  enum mpd_state state = mpd_status_get_state( status );

  switch ( state ) {
  case MPD_STATE_PLAY:
    current.play_status = MPD_PLAY_STATUS_PLAYING;
    break;
  case MPD_STATE_PAUSE:
    current.play_status = MPD_PLAY_STATUS_PAUSED;
    break;
  case MPD_STATE_STOP:
    current.play_status = MPD_PLAY_STATUS_STOPPED;
    break;
  case MPD_STATE_UNKNOWN:
    current.play_status = MPD_PLAY_STATUS_NOSONG;
    break;
  default:
    break;
  }

  // I can already tell this needs some rearrangment.
  previous->changed = 0;
  if ( previous->play_status != current.play_status ) {
    previous->play_status = current.play_status;
    previous->changed |= MPD_CHANGED_STATUS;
  }
  if ( previous->elapsed_time != mpd_status_get_elapsed_time( status ) ) {
    previous->elapsed_time = mpd_status_get_elapsed_time( status );
    previous->changed |= MPD_CHANGED_ELAPSED;
  }
  if ( previous->total_time != mpd_status_get_total_time( status ) ) {
    previous->total_time = mpd_status_get_total_time( status );
    previous->changed |= MPD_CHANGED_TOTAL;
  }

  mpd_status_free( status );

  mpd_response_next( connection );

  struct mpd_song* song = NULL;

  // Can there really be more than one song playing? Do we care?
  // The command was "send_current_song" which seems to imply
  // only one.
  while ( ( song = mpd_recv_song( connection ) ) != NULL ) {
    get_song_string( song, MPD_TAG_ARTIST, current.artist );
    get_song_string( song, MPD_TAG_ALBUM, current.album );
    get_song_string( song, MPD_TAG_TITLE, current.title );
    mpd_song_free( song );
  }

  if ( ! g_string_equal( previous->artist, current.artist ) ) {
    g_string_assign( previous->artist, current.artist->str );
    previous->changed |= MPD_CHANGED_ARTIST;
  }
  if ( ! g_string_equal( previous->album, current.album ) ) {
    g_string_assign( previous->album, current.album->str );
    previous->changed |= MPD_CHANGED_ALBUM;
  }
  if ( ! g_string_equal( previous->title, current.title ) ) {
    g_string_assign( previous->title, current.title->str );
    previous->changed |= MPD_CHANGED_TITLE;
  }

  if ( ( mpd_connection_get_error( connection ) != MPD_ERROR_SUCCESS ) ||
       ! mpd_response_finish( connection ) ) {
    log_message_error( logger, "Well, something went wrong with the MPD command list." );

    mpd_current_free( &current );

    return -1;
  }

  mpd_current_free( &current ); // This is something of an annoyance.

  return 0;
}

struct MPD_HANDLE mpd_create ( const char* host, int port,
			       struct LOG_HANDLE logger )
{
  struct MPD_HANDLE handle;
  handle.d = malloc( sizeof( struct MPD_PRIVATE ) );
  handle.d->fd   = -1;
  handle.d->host = g_string_new( host );
  handle.d->port = port;
  handle.d->logger = logger;

  mpd_current_init( &handle.d->current );

  handle.d->connection = mpd_connection_new( handle.d->host->str,
					     handle.d->port,
					     0 ); // <- timeout: 0->default

  if ( mpd_connection_get_error( handle.d->connection ) != MPD_ERROR_SUCCESS ) {
    log_message_error( handle.d->logger, "",
		       mpd_connection_get_error_message( handle.d->connection ) );
  }
  else {
    handle.d->fd = mpd_connection_get_fd( handle.d->connection );
  }

  return handle;
}

int mpd_reconnect ( struct MPD_HANDLE handle )
{
  // \bug This is probably not right now. Reconnecting is not quite
  // as simple as closing the file descriptor. And it is not clear
  // in what context this function is invoked...

  if ( handle.d == NULL ) {
    return -1;
  }

  log_message_info( handle.d->logger, "starting new connection." );
  handle.d->connection = mpd_connection_new( handle.d->host->str,
					     handle.d->port,
					     0 ); // <- timeout: 0->default
  log_message_info( handle.d->logger, "new connection returned %x.", 
		    handle.d->connection );

  enum mpd_error well = mpd_connection_get_error( handle.d->connection );
  if ( well == MPD_ERROR_SUCCESS ) {
    log_message_info( handle.d->logger, "appears to have worked" );
  }
  else {
    log_message_info( handle.d->logger, "meh: %s", mpd_connection_get_error_message( handle.d->connection ) );
  }
#if 0
  return mpd_status( handle );
#else
  if ( well != MPD_ERROR_SUCCESS )
    return -1;
  return 0;
#endif
}

int mpd_status ( const struct MPD_HANDLE handle )
{
  int status = -1;
  if ( handle.d != 0 ) {
    status = handle.d->fd; // Not much of a status.
  }
  return status;
}

void mpd_free ( struct MPD_HANDLE handle )
{
  if ( handle.d != NULL ) {
    mpd_current_free( &handle.d->current );

    g_string_free( handle.d->host, TRUE );
    if ( handle.d->fd > -1 ) {
      close(  handle.d->fd );
    }
    free( handle.d );
    handle.d = NULL;
  }
}

int mpd_poll ( struct MPD_HANDLE handle )
{
  int status = -1;
  if ( handle.d != 0 ) {
    status = mpd_get_current( handle.d->connection,
			      handle.d->logger, &handle.d->current );
  }
  return status;
}

bool mpd_changed ( const struct MPD_HANDLE handle, int flags )
{
  bool changed = false;
  if ( handle.d != 0 ) {
    changed = handle.d->current.changed & flags;
  }
  return changed;
}

enum MPD_PLAY_STATUS mpd_play_status ( const struct MPD_HANDLE handle )
{
  enum MPD_PLAY_STATUS status = MPD_PLAY_STATUS_NOSONG;
  if ( handle.d != 0 ) {
    status = handle.d->current.play_status;
  }
  return status;
}

char* mpd_artist ( const struct MPD_HANDLE handle )
{
  char* artist = 0;
  if ( handle.d != 0 ) {
    artist = handle.d->current.artist->str;
  }
  return artist;
}

char* mpd_album ( const struct MPD_HANDLE handle )
{
  char* album = 0;
  if ( handle.d != 0 ) {
    album = handle.d->current.album->str;
  }
  return album;
}

char* mpd_title ( const struct MPD_HANDLE handle )
{
  char* title = 0;
  if ( handle.d != 0 ) {
    title = handle.d->current.title->str;
  }
  return title;
}

struct MPD_TIMES mpd_times ( const struct MPD_HANDLE handle )
{
  struct MPD_TIMES times = { 0, 0 };
  if ( handle.d != 0 ) {
    times.elapsed = handle.d->current.elapsed_time;
    times.total   = handle.d->current.total_time;
  }
  return times;
}

void mpd_play_pause ( struct MPD_HANDLE handle )
{
  if ( handle.d != 0 ) {
    struct mpd_connection* connection = handle.d->connection;

    mpd_send_toggle_pause( connection );

    if ( ( mpd_connection_get_error( connection ) != MPD_ERROR_SUCCESS ) ||
	 ! mpd_response_finish( connection ) ) {
      
      log_message_error( handle.d->logger, 
			 "Well, something went wrong with the MPD play/pause toggle." );
    }
  }
}
