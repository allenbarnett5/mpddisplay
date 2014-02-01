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
#include <unistd.h>
#include <stdbool.h>

#include "mpd_intf.h"

static ssize_t read_line( int fd, void* buffer, size_t n );
static int put_line ( int fd, void* buffer, size_t n );

int mpd_connect ( const char* host, const char* port )
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
    }
  }

  return mpd;
}

void mpd_current_init ( struct MPD_CURRENT* current )
{
  current->changed = 0;
  current->play_status = MPD_PLAY_STATUS_NOSONG;
  firestring_estr_alloc( &current->artist, 256 );
  firestring_estr_alloc( &current->album, 256 );
  firestring_estr_alloc( &current->title, 256 );
  current->elapsed_time = 0;
  current->total_time = 0;
}

void mpd_current_free ( struct MPD_CURRENT* current )
{
  firestring_estr_free( &current->artist );
  firestring_estr_free( &current->album );
  firestring_estr_free( &current->title );
}

int mpd_get_current ( int mpd, struct MPD_CURRENT* previous )
{
  // So, this is the nub of it. Send a command to MPD and await its
  // response.
  // \bug could compress this down into a single call to put_line...
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
#if 0
    printf( "SR: %s", buffer );
#endif
    if ( n_read > 1 && strncmp( "OK", buffer, 2 ) == 0 ) {
      break;
    }
    else if ( n_read > 2 && strncmp( "ACK", buffer, 3 ) == 0 ) {
      printf( "Error from MPD: '%s'\n", buffer );
      break;
    }
    // The rest of this is parsing the output.
    else if ( n_read > 6 && strncmp( "Artist:", buffer, 7 ) == 0 ) {
      firestring_estr_astrcpy( &current.artist, &buffer[7] );
      firestring_estr_ip_trim( &current.artist );
      continue;
    }
    else if ( n_read > 5 && strncmp( "Album:", buffer, 6 ) == 0 ) {
      firestring_estr_astrcpy( &current.album, &buffer[6] );
      firestring_estr_ip_trim( &current.album );
      continue;
    }
    else if ( n_read > 5 && strncmp( "Title:", buffer, 6 ) == 0 ) {
      firestring_estr_astrcpy( &current.title, &buffer[6] );
      firestring_estr_ip_trim( &current.title );
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
  if ( firestring_estr_estrcmp( &previous->artist, &current.artist, 0 ) != 0 ) {
    firestring_estr_aestrcpy( &previous->artist, &current.artist, 0 );
    previous->changed |= MPD_CHANGED_ARTIST;
  }
  if ( firestring_estr_estrcmp( &previous->album, &current.album, 0 ) != 0 ) {
    firestring_estr_aestrcpy( &previous->album, &current.album, 0 );
    previous->changed |= MPD_CHANGED_ALBUM;
  }
  if ( firestring_estr_estrcmp( &previous->title, &current.title, 0 ) != 0 ) {
    firestring_estr_aestrcpy( &previous->title, &current.title, 0 );
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

void mpd_close ( int mpd )
{
  // I guess this could fail, but what do you do? There's no state we
  // care about preserving. \bug Maybe we should send the "close"
  // command. Not that MPD appears to care, though.
  close( mpd );
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
