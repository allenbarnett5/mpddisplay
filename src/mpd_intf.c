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
#if 0
  //! "service" string (usually a port number, though).
  GString* port;
#else
  //! Port number (libmpdclient doesn't accept "service")
  int port;
#endif
#if 0
  //! Query string.
  GString* query;
#endif
  //! Our logging object.
  struct LOG_HANDLE logger;
  //! The results of the last poll of the server.
  struct MPD_CURRENT current;
};

#if 0
static ssize_t read_line( int fd, void* buffer, size_t n );
#endif
#if 0
static int put_line ( int fd, void* buffer, size_t n );
#endif
#if 0
static int mpd_connect ( const char* host, const char* port,
			 struct LOG_HANDLE logger )
{
  int mpd = -1; // Assume the worst.

  // Try to find the daemon on the remote port.
  struct addrinfo hints;
  struct addrinfo* addrs;
  memset( &hints, 0, sizeof hints );
  hints.ai_family   = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags    = AI_NUMERICSERV;

  errno = 0;

  int ret = getaddrinfo( host, port, &hints, &addrs );

  if ( ret != 0 ) {
    log_message_error( logger,
		       "getaddrinfo failed trying to look up '%s' port '%s': %s",
		       host, port, gai_strerror( ret ) );
    if ( ret == EAI_SYSTEM ) {
      log_message_error( logger, "Additionally: %s", strerror( errno ) );
    }
    return mpd;
  }

  struct addrinfo* addr;

  for ( addr = addrs; addr != NULL; addr = addr->ai_next ) {

    errno = 0;

    mpd = socket( addr->ai_family, addr->ai_socktype, addr->ai_protocol );

    if ( mpd == -1 ) {
      log_message_warn(  logger,
			 "Could not create socket for addrinfo: %s",
			 strerror( errno ) );
      continue;
    }

    char addr_host[1025], addr_serv[32];
    getnameinfo( addr->ai_addr, addr->ai_addrlen,
		 addr_host, sizeof addr_host,
		 addr_serv, sizeof addr_serv, 0 );

    log_message_info( logger, "Connecting to %s %s", addr_host, addr_serv );

    errno = 0;

    if ( connect( mpd, addr->ai_addr, addr->ai_addrlen ) != -1 ) {
      break;
    }
    else {
      log_message_warn( logger, "Could not connect: %s",
			strerror( errno ) );
    }

    close( mpd );

    mpd = -1;
  }

  freeaddrinfo( addrs );

  if ( addr == NULL ) {
    log_message_error( logger, "Could not connect socket to any address" );
  }
  else if ( mpd >= 0 ) {
    // Peek at the first line from the server. We expect it to
    // be the string "OK MPD".
    char buffer[32];
    ssize_t n = read_line( mpd, buffer, sizeof buffer );
    if ( n < 0 ) {
      log_message_error( logger,
			 "Failed to hear from server: %s",
			 strerror( errno ) );
      close( mpd );
      mpd = -1;
    }
    else {
      if ( strncmp( "OK MPD", buffer, 6 ) != 0 ) {
	log_message_error( logger,
			   "Did not receive correct response from server\n\
Expecting \"OK MPD\", instead received \"%s\"", buffer );
	close( mpd );
	mpd = -1;
      }
      else {
	log_message_info( logger, "Connection successful on fd: %d", mpd );
      }
    }
  }

  return mpd;
}
#endif
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
  while ( ( value = mpd_song_get_tag( song, type, i++ ) ) != NULL ) {
    g_string_append( result, value );
  }
}

#if 0
static int mpd_get_current ( int mpd, const GString* query,
			     struct LOG_HANDLE logger,
			     struct MPD_CURRENT* previous )
#else
static int mpd_get_current ( struct mpd_connection* connection,
			     struct LOG_HANDLE logger,
			     struct MPD_CURRENT* previous )
#endif
{
  // So, this is the nub of it. Send a command to MPD and await its
  // response.
#if 0
  int n_written;

  n_written = put_line( mpd, query->str, query->len );

  if ( n_written != (int)query->len ) {
    log_message_error( logger, "Well, something went wrong with the command list. Probably lost the connection." );
    return -1;
  }

  int n_read;
  char buffer[1024];
#endif
  struct MPD_CURRENT current;
  mpd_current_init( &current );
#if 0
  errno = 0;
  while ( ( n_read = read_line( mpd, buffer, sizeof buffer ) ) != -1 ) {
    // \bug if n_read == 0, then we got EOF on the socket. Which is kind of
    // an error...
    if ( n_read == 0 ) {
      return -1;
    }

    if ( n_read > 1 && strncmp( "OK", buffer, 2 ) == 0 ) {
      break;
    }
    else if ( n_read > 2 && strncmp( "ACK", buffer, 3 ) == 0 ) {
      log_message_error( logger, "Message from MPD: '%s'", buffer );
      break;
    }
    // The rest of this is parsing the output.
    else if ( n_read > 7 && strncmp( "Artist: ", buffer, 8 ) == 0 ) {
      g_string_assign( current.artist, &buffer[8] );
      continue;
    }
    else if ( n_read > 6 && strncmp( "Album: ", buffer, 7 ) == 0 ) {
      if ( current.album->len > 0 ) {
	g_string_append( current.album, " - " );
	g_string_append( current.album, &buffer[7] );
      }
      else {
	g_string_assign( current.album, &buffer[7] );
      }
      continue;
    }
    else if ( n_read > 6 && strncmp( "Title: ", buffer, 7 ) == 0 ) {
      if ( current.title->len > 0 ) {
	g_string_append( current.title, " - " );
	g_string_append( current.title, &buffer[7] );
      }
      else {
	g_string_assign( current.title, &buffer[7] );
      }
      continue;
    }
    else if ( n_read > 4 && strncmp( "time:", buffer, 5 ) == 0 ) {
      // Parse out the two times...
      int elapsed_time, total_time;
      int n_values = sscanf( &buffer[5], " %d : %d",
			     &elapsed_time, &total_time );
      if ( n_values == 2 ) {
	current.elapsed_time = elapsed_time;
	current.total_time = total_time;
      }
      continue;
    }
    else if ( n_read > 5 && strncmp( "state:", buffer, 6 ) == 0 ) {
      // Parse out the state...
      // \bug this could be more defensive
      if ( strncmp( &buffer[7], "play", 4 ) == 0 ) {
	current.play_status = MPD_PLAY_STATUS_PLAYING;
      }
      else if ( strncmp( &buffer[7], "pause", 5 ) == 0 ) {
	current.play_status = MPD_PLAY_STATUS_PAUSED;
      }
      else if ( strncmp( &buffer[7], "stop", 4 ) == 0 ) {
	current.play_status = MPD_PLAY_STATUS_STOPPED;
      }
      continue;
    }
  }

  if ( n_read == -1 ) {
    log_message_error( logger, "Well, something went wrong with the read. Probably lost the connection." );
    mpd_current_free( &current );
    return -1;
  }
#else
  mpd_command_list_begin( connection, true );
  mpd_send_status( connection );
  mpd_send_current_song( connection );
  mpd_command_list_end( connection );

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
#endif
#if 0
  previous->changed = 0;
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
  if ( previous->elapsed_time != current.elapsed_time ) {
    previous->elapsed_time = current.elapsed_time;
    previous->changed |= MPD_CHANGED_ELAPSED;
  }
  if ( previous->total_time != current.total_time ) {
    previous->total_time = current.total_time;
    previous->changed |= MPD_CHANGED_TOTAL;
  }
  if ( previous->play_status != current.play_status ) {
    previous->play_status = current.play_status;
    previous->changed |= MPD_CHANGED_STATUS;
  }
#endif
  mpd_current_free( &current ); // This is something of an annoyance.

  return 0;
}
#if 0
static ssize_t read_line( int fd, void* buffer, size_t n )
{
  ssize_t n_read; /* # of byes fetched by last read */
  size_t total_read; /* Total read so far */
  char* c_buf;
  char c;
  if ( n <= 0 || buffer == NULL ) {
    errno = EINVAL;
    return -1;
  }
  c_buf  = buffer; /* No pointer arithmetic on void* */
  total_read = 0;
  for ( ;; ) {
    n_read = read( fd, &c, 1 );
    if ( n_read == -1 ) {
      if ( errno == EINTR ) /* Interrupted -> restart read() */ {
	continue;
      }
      else {
	return -1; /* Some other error */
      }
    }
    else if ( n_read == 0 ) {
      if ( total_read == 0 ) {
	/* No bytes read; return 0 */
	return 0;
      }
      else {
	/* Some bytes read; add \0 */
	break;
      }
    }
    else {
      /* n_read must be 1 if we get here */
      if ( total_read < n - 1 ) {
	total_read++;
	*c_buf++ = c;
      }
      if ( c == '\n' ) {
	// I think we want to eat the new line.
	c_buf--;
	total_read--;
	break;
      }
    }
  }
  *c_buf = '\0';
  return total_read;
}
#endif
#if 0
static int put_line ( int fd, void* buffer, size_t n )
{
  ssize_t n_written;
  size_t total_written;
  char* c_buf;
  
  if ( n <= 0 || buffer == NULL ) {
    errno = EINVAL;
    return -1;
  }

  c_buf = buffer;
  total_written = 0;

  for ( ;; ) {
    n_written = write( fd, c_buf, n );

    if ( n_written == -1 ) {
      if ( errno == EINTR ) {
	continue;
      }
      else {
	return -1;
      }
    }
    else if ( (size_t)n_written < n ) {
      c_buf += n_written;
      n -= n_written;
      total_written += n_written;
    }
    else {
      total_written += n_written;
      break; // Write complete.
    }
  }

  return total_written;
}
#endif
#if 0
struct MPD_HANDLE mpd_create ( const char* host, const char* port,
			       struct LOG_HANDLE logger )
#else
struct MPD_HANDLE mpd_create ( const char* host, int port,
			       struct LOG_HANDLE logger )
#endif
{
  struct MPD_HANDLE handle;
  handle.d = malloc( sizeof( struct MPD_PRIVATE ) );
  handle.d->fd   = -1;
  handle.d->host = g_string_new( host );
#if 0
  handle.d->port = g_string_new( port );
#else
  handle.d->port = port;
#endif
#if 0
  handle.d->query = g_string_new( "\
command_list_begin\n\
status\n\
currentsong\n\
command_list_end\n" );
#endif
  handle.d->logger = logger;

  mpd_current_init( &handle.d->current );
#if 0
  handle.d->fd = mpd_connect( handle.d->host->str, handle.d->port->str,
			      handle.d->logger );
#else
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
#endif
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

#if 0
  close( handle.d->fd );
  handle.d->fd = mpd_connect( handle.d->host->str, handle.d->port->str,
			      handle.d->logger );
#else
  handle.d->connection = mpd_connection_new( handle.d->host->str,
					     handle.d->port,
					     0 ); // <- timeout: 0->default
#endif

  return mpd_status( handle );
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
#if 0
    g_string_free( handle.d->port, TRUE );
#endif
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
#if 0
    status = mpd_get_current( handle.d->fd, handle.d->query,
			      handle.d->logger, &handle.d->current );
#else
    status = mpd_get_current( handle.d->connection,
			      handle.d->logger, &handle.d->current );
#endif
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
#if 0
  if ( handle.d != 0 ) {
    GString* command = g_string_new( "" );
    put_line( handle.d->fd, command->str, command->len );
    g_string_free( command, TRUE );
  }
#else
  if ( handle.d != 0 ) {
    struct mpd_connection* connection = handle.d->connection;

    mpd_send_toggle_pause( connection );

    if ( ( mpd_connection_get_error( connection ) != MPD_ERROR_SUCCESS ) ||
	 ! mpd_response_finish( connection ) ) {
      
      log_message_error( handle.d->logger, 
			 "Well, something went wrong with the MPD play/pause toggle." );
    }
  }
#endif
}
