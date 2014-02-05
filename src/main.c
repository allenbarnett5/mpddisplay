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

static gboolean poll_mpd ( gpointer data );
static gboolean reconnect_mpd ( gpointer data );

const char* USAGE = "usage: %s [--host hostname] [--port port#]\n";

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

  int ret = display_init();

  if ( ret == -1 ) {
    return 1;
  }

  // Well, after all that, we can now start polling MPD to see what's up.

  mpd_current_init( &main_data.current );

  main_data.loop = g_main_loop_new( NULL, FALSE );

  (void)g_timeout_add_seconds( 1, poll_mpd, &main_data );

  g_main_loop_run( main_data.loop );

  g_main_loop_unref( main_data.loop );

  mpd_close( main_data.mpd );

  display_close();

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

gboolean reconnect_mpd ( gpointer data )
{
  // \bug this is not right, of couse, we have to figure out
  // if we can call mpd_connect again.
  printf( "Connected to MPD again!\n" );
  (void)g_timeout_add_seconds( 1, poll_mpd, data );
  return FALSE;
}

gboolean poll_mpd ( gpointer data )
{
  struct MAIN_DATA* main_data = data;

  int status = mpd_get_current( main_data->mpd, &main_data->current );

  if ( status != 0 ) {
    printf( "We lost our connection to MPD. Trying again shortly.\n" );
    (void)g_timeout_add_seconds( 5, reconnect_mpd, data );
    return FALSE;
  }

  if ( main_data->current.changed & MPD_CHANGED_ANY ) {
    if ( display_update( &main_data->current ) < 0 ) {
      // \bug well, what should I do here? Muddle on for now.
      return TRUE;
    }
  }

  return TRUE;
}
