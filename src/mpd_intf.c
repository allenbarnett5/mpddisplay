/*
 * Encapsulate the interaction with MPD. Because this is a network
 * application, there is a lot which can go wrong.
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
  int elapsed_time;
  //! Total track time in seconds.
  int total_time;
};
/*!
 * The details of the MPD connection.
 */
struct MPD_PRIVATE {
  //! The socket file descriptor.
  int fd;
  //! Host string.
  GString* host;
  //! "service" string (usually a port number, though).
  GString* port;
  //! The results of the last poll of the server.
  struct MPD_CURRENT current;
};


static ssize_t read_line( int fd, void* buffer, size_t n );
static int put_line ( int fd, void* buffer, size_t n );

static int mpd_connect ( const char* host, const char* port )
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
    printf( "Error: getaddrinfo failed trying to look up '%s' port '%s': %s\n",
	    host, port, gai_strerror( ret ) );
    if ( ret == EAI_SYSTEM ) {
      printf( "\tAdditionally: %s\n", strerror( errno ) );
    }
    return mpd;
  }

  struct addrinfo* addr;

  for ( addr = addrs; addr != NULL; addr = addr->ai_next ) {

    errno = 0;

    mpd = socket( addr->ai_family, addr->ai_socktype, addr->ai_protocol );

    if ( mpd == -1 ) {
      printf( "Warning: Could not create socket for addrinfo: %s\n",
	      strerror( errno ) );
      continue;
    }

    char addr_host[1025], addr_serv[32];
    getnameinfo( addr->ai_addr, addr->ai_addrlen,
		 addr_host, sizeof addr_host,
		 addr_serv, sizeof addr_serv, 0 );

    printf( "Info: Connecting to %s %s\n", addr_host, addr_serv );

    errno = 0;

    if ( connect( mpd, addr->ai_addr, addr->ai_addrlen ) != -1 ) {
      break;
    }
    else {
      printf( "Warning: Could not connect: %s\n", strerror( errno ) );
    }

    close( mpd );

    mpd = -1;
  }

  freeaddrinfo( addrs );

  if ( addr == NULL ) {
    printf( "Error: Could not connect socket to any address\n" );
  }
  else if ( mpd >= 0 ) {
    // Peek at the first line from the server. We expect it to
    // be the string "OK MPD".
    char buffer[32];
    ssize_t n = read_line( mpd, buffer, sizeof buffer );
    if ( n < 0 ) {
      printf( "Error: Failed to hear from server: %s\n", strerror( errno ) );
      close( mpd );
      mpd = -1;
    }
    else {
      if ( strncmp( "OK MPD", buffer, 6 ) != 0 ) {
	printf( "Error: Did not receive correct response from server\n" );
	printf( "Expecting \"OK MPD\", instead received \"%s\"\n", buffer );
	close( mpd );
	mpd = -1;
      }
      else {
	printf( "Connection successful on fd: %d\n", mpd );
      }
    }
  }

  return mpd;
}

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

static int mpd_get_current ( int mpd, struct MPD_CURRENT* previous )
{
  // So, this is the nub of it. Send a command to MPD and await its
  // response.
  // \bug could compress this down into a single call to put_line...
  // \bug construct a string with all this to begin with...
  int n_written;

  n_written = put_line( mpd, "command_list_begin\n", 19 );

  if ( n_written != 19 ) {
    printf( "Well, something went wrong with the command_list_begin. Probably lost the connection.\n" );
    return -1;
  }

  n_written = put_line( mpd, "status\n", 7 );

  if ( n_written != 7 ) {
    printf( "Well, something went wrong with the status. Probably lost the connection.\n" );
    return -1;
  }

  n_written = put_line( mpd, "currentsong\n", 12 );

  if ( n_written != 12 ) {
    printf( "Well, something went wrong with the currentsong. Probably lost the connection.\n" );
    return -1;
  }

  n_written = put_line( mpd, "command_list_end\n", 17 );

  if ( n_written != 17 ) {
    printf( "Well, something went wrong with the command_list_end. Probably lost the connection.\n" );
    return -1;
  }

  int n_read;
  char buffer[1024];

  struct MPD_CURRENT current;
  mpd_current_init( &current );

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
      printf( "Error from MPD: '%s'\n", buffer );
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
    printf( "Well, something went wrong with the read. Probably lost the connection.\n" );
    mpd_current_free( &current );
    return -1;
  }

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

  mpd_current_free( &current ); // This is something of an annoyance.

  return 0;
}

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

struct MPD_HANDLE mpd_create ( const char* host, const char* port )
{
  struct MPD_HANDLE handle;
  handle.d = malloc( sizeof( struct MPD_PRIVATE ) );
  handle.d->fd   = -1;
  handle.d->host = g_string_new( host );
  handle.d->port = g_string_new( port );

  mpd_current_init( &handle.d->current );

  handle.d->fd = mpd_connect( handle.d->host->str, handle.d->port->str );

  return handle;
}

int mpd_reconnect ( struct MPD_HANDLE handle )
{
  if ( handle.d == NULL ) {
    return -1;
  }

  close( handle.d->fd );
  handle.d->fd = mpd_connect( handle.d->host->str, handle.d->port->str );

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
    g_string_free( handle.d->port, TRUE );
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
    status = mpd_get_current( handle.d->fd, &handle.d->current );
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
