/*
 * MPD Display: Connect to a Music Player Daemon and display the
 * information about the song currently playing. For the Raspberry Pi.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "glib.h"

#include "mpd_intf.h"
#include "display_intf.h"

static int convert_int ( const char* string );
#if 0
static int idle ( int seconds );
#endif
static gboolean poll_mpd ( gpointer data );
static gboolean reconnect_mpd ( gpointer data );

const char* USAGE = "usage: %s [--host hostname] [--port port#]\n";

// \bug Probably should let the user choose.
const char* MPD_DISPLAY_FONT = "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf";

struct MAIN_DATA {
  int mpd;
  struct MPD_CURRENT current;
  GMainLoop* loop;
} main_data;

int main ( int argc, char* argv[] )
{
  // We have to be told where MPD is running.
  int c;
  char* host = "guanaco"; // Well, that's mine. Maybe this should be localhost.
  char* port = "6600";    // The standard MPD port.
  bool bad_argument = false;

  while ( 1 ) {
    int option_index = 0;
    static struct option long_options[] = {
      { "host", required_argument, 0, 'h' },
      { "port", required_argument, 0, 'p' },
      { 0,      0,                 0, 0 }
    };

    c = getopt_long( argc, argv, "h:p:", long_options, &option_index );

    if ( c == -1 ) {
      break;
    }

    switch ( c ) {
    case 'h':
      if ( *optarg == '\0' ) {
	bad_argument = true;
	printf( "--host argument must be non-empty\n" );
      }
      host = optarg;
      break;
    case 'p':
      port = optarg;
      {
	int port_n = convert_int( optarg );
	if ( errno != 0 ) {
	  bad_argument = true;
	  printf( "--port argument was not a valid integer: '%s' (%s)\n",
		  optarg, strerror( errno ) );
	}
	if ( port_n <= 0 ) {
	  bad_argument = true;
	  printf( "--port argument must be positive (%s)\n", port );
	}
      }
      break;
    default:
      bad_argument = true;
      printf( "?? getopt returned character code 0%o ??\n", c );
    }
  }
  if ( bad_argument ) {
    printf( USAGE, argv[0] );
    return 1;
  }

  printf( "MPD host: '%s'\n", host );
  printf( "MPD port: '%s'\n", port );

  main_data.mpd = mpd_connect( host, port );

  if ( main_data.mpd < 0 ) {
    printf( USAGE, argv[0] );
    return 1;
  }

  // If we get this far, we can try to initialize the graphics.

  int ret = display_init( MPD_DISPLAY_FONT );

  if ( ret == -1 ) {
    return 1;
  }

  // Well, after all that, we can now start polling MPD to see what's up.

  mpd_current_init( &main_data.current );
#if 0
  int status = 0;

  while ( status == 0 ) {

    if( idle( 1 ) < 0 ) {
      printf( "Error: Failed to sleep: %s\n", strerror( errno ) );
    }

    status = mpd_get_current( mpd, &current );

    if ( status == 0 ) {
#if 0
      if ( current.changed & MPD_CHANGED_ARTIST ) {
	firestring_printf( "new artist: '%e'\n", &current.artist );
      }
      if ( current.changed & MPD_CHANGED_ALBUM ) {
	firestring_printf( "new album: '%e'\n", &current.album );
      }
      if ( current.changed & MPD_CHANGED_TITLE ) {
	firestring_printf( "new title: '%e'\n", &current.title );
      }
      if ( current.changed & MPD_CHANGED_ELAPSED ) {
	firestring_printf( "new elapsed time: %d\n", current.elapsed_time );
      }
      if ( current.changed & MPD_CHANGED_TOTAL ) {
	firestring_printf( "new total time: %d\n", current.total_time );
      }
      if ( current.changed & MPD_CHANGED_STATUS ) {
	firestring_printf( "new status: %d\n", current.play_status );
      }
#endif
    }
    else {
      return 1;
    }

    if ( current.changed & MPD_CHANGED_ANY ) {
      if ( display_update( &current ) < 0 ) {
	return 1;
      }
    }
  }

  mpd_close( mpd );

  display_close();
#else
  main_data.loop = g_main_loop_new( NULL, FALSE );

  (void)g_timeout_add_seconds( 1, poll_mpd, &main_data );

  g_main_loop_run( main_data.loop );

  g_main_loop_unref( main_data.loop );

  mpd_close( main_data.mpd );

  display_close();
#endif

  return 0;
}

/*!
 * Converts a string to an integer. The string can have leading
 * whitespace (since strtol allows it), but the number must terminate
 * at the end of the string. errno is set if the conversion fails.
 * \param[in] string the string to convert to an integer.
 * \return the integer. If errno is not 0, then the value is
 * indeterminate.
 */
static int convert_int ( const char* string )
{
  errno = 0;
  char* tail;
  int value = strtol( string, &tail, 0 );
  // Must consume the entire string.
  if ( *tail != '\0' ) {
    errno = EINVAL;
  }
  return value;
}
#if 0
/*!
 * Sleep for the given number of seconds.
 * \param[0] seconds time to sleep in seconds.
 * \return 0 if we successfully waited, otherwise return -1. There is
 * a remote possibility that this could fail. Indicates a programming
 * error probably.
 */
static int idle ( int seconds )
{
  // I guess this is a best effort. If it fails, we just keep
  // trying.
  struct timespec request = { .tv_sec = seconds, .tv_nsec = 0 };
  struct timespec remainder;
  for ( ;; ) {
    errno = 0;

    int ret = nanosleep( &request, &remainder );

    if ( ret == 0 ) {
      break;
    }
    else {
      if ( errno == EINTR ) {
	request = remainder;
      }
      else {
	// What to do here? It means our call was not correct. Probably
	// should exit the program since we're not using nanosleep
	// correctly.
	return -1;
      }
    }
  }
  return 0;
}
#endif
gboolean reconnect_mpd ( gpointer data )
{
#if 0
  static int count = 0;
  if ( ++count < 5 ) {
    printf( "Trying to reconnect to MPD %d\n", count );
    return TRUE;
  }
  else {
    printf( "Connected to MPD again!\n" );
    (void)g_timeout_add_seconds( 1, poll_mpd, data );
    count = 0;
    return FALSE;
  }
#else
  // \bug this is not right, of couse, we have to figure out
  // if we can call mpd_connect again.
  printf( "Connected to MPD again!\n" );
  (void)g_timeout_add_seconds( 1, poll_mpd, data );
  return FALSE;
#endif
}

gboolean poll_mpd ( gpointer data )
{
#if 0
  static int count = 0;

  if ( ++count < 10 ) {
    printf( "Read from MPD %d.\n", count );
    return TRUE;
  }
  else {
    printf( "We lost our connection to MPD. Trying again shortly.\n" );
    (void)g_timeout_add_seconds( 2, reconnect_mpd, data );
    count = 0;
    return FALSE;
  }
#else
  struct MAIN_DATA* main_data = data;

  int status = mpd_get_current( main_data->mpd, &main_data->current );

  if ( status == 0 ) {
#if 0
    if ( current.changed & MPD_CHANGED_ARTIST ) {
      firestring_printf( "new artist: '%e'\n", &current.artist );
    }
    if ( current.changed & MPD_CHANGED_ALBUM ) {
      firestring_printf( "new album: '%e'\n", &current.album );
    }
    if ( current.changed & MPD_CHANGED_TITLE ) {
      firestring_printf( "new title: '%e'\n", &current.title );
    }
    if ( current.changed & MPD_CHANGED_ELAPSED ) {
      firestring_printf( "new elapsed time: %d\n", current.elapsed_time );
    }
    if ( current.changed & MPD_CHANGED_TOTAL ) {
      firestring_printf( "new total time: %d\n", current.total_time );
    }
    if ( current.changed & MPD_CHANGED_STATUS ) {
      firestring_printf( "new status: %d\n", current.play_status );
    }
#endif
  }
  else {
    printf( "We lost our connection to MPD. Trying again shortly.\n" );
    (void)g_timeout_add_seconds( 5, reconnect_mpd, data );
    return FALSE;
  }

  if ( main_data->current.changed & MPD_CHANGED_ANY ) {
    if ( display_update( &main_data->current ) < 0 ) {
      // \bug well, what should I do here?
      return TRUE;
    }
  }

  return TRUE;
#endif
}
