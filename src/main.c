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
#include "log_intf.h"
#include "cover_image.h"

static int convert_int ( const char* string );

static gboolean poll_mpd ( gpointer data );
static gboolean reconnect_mpd ( gpointer data );

const char* USAGE = "usage: %s [--host hostname] [--port port#] [--database databse]\n";

struct MAIN_DATA {
  struct MPD_HANDLE mpd;
  struct LOG_HANDLE logger;
  struct IMAGE_DB_HANDLE image_db;
  GMainLoop* loop;
} main_data;

int main ( int argc, char* argv[] )
{
  // We have to be told where MPD is running.
  int c;
  char* host = "guanaco"; // Well, that's mine. Maybe this should be localhost.
  char* port = "6600";    // The standard MPD port.
  char* database = "album_art.sqlite3";
  bool bad_argument = false;

  // Start the logger.
  main_data.logger = log_init();

  while ( 1 ) {
    int option_index = 0;
    static struct option long_options[] = {
      { "host", required_argument, 0, 'h' },
      { "port", required_argument, 0, 'p' },
      { "database", required_argument, 0, 'd' },
      { 0,      0,                 0, 0 }
    };

    c = getopt_long( argc, argv, "h:p:d:", long_options, &option_index );

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
    case 'd':
      if ( *optarg == '\0' ) {
	bad_argument = true;
	printf( "--database argument must be non-empty\n" );
      }
      database = optarg;
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

  log_message_info( main_data.logger, "MPD host: '%s'", host );
  log_message_info( main_data.logger, "MPD port: '%s'", port );
  log_message_info( main_data.logger, "Database: '%s'", database );

  main_data.mpd = mpd_create( host, port, main_data.logger );

  if ( mpd_status( main_data.mpd ) < 0 ) {
    printf( USAGE, argv[0] );
    return 1;
  }

  // Try to open the image database connection.

  main_data.image_db = image_db_create( database, main_data.logger );

  // If we get this far, we can try to initialize the graphics.

  int ret = display_init( main_data.image_db );

  if ( ret == -1 ) {
    return 1;
  }

  // Well, after all that, we can now start polling MPD to see what's up.

  main_data.loop = g_main_loop_new( NULL, FALSE );

  (void)g_timeout_add_seconds( 1, poll_mpd, &main_data );

  g_main_loop_run( main_data.loop );

  g_main_loop_unref( main_data.loop );

  display_close();

  mpd_free( main_data.mpd );

  log_close( main_data.logger );

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
  struct MAIN_DATA* main_data = data;
  int status;
  log_message_warn( main_data->logger, "Reconnecting to MPD again!" );

  status = mpd_reconnect( main_data->mpd );

  if ( status < 0 ) {
    log_message_error( main_data->logger, "Reconnection failed" );
    return TRUE;
  }
  (void)g_timeout_add_seconds( 1, poll_mpd, data );
  return FALSE;
}

gboolean poll_mpd ( gpointer data )
{
  struct MAIN_DATA* main_data = data;

  int status = mpd_poll( main_data->mpd );

  if ( status != 0 ) {
    log_message_warn( main_data->logger,
		      "We lost our connection to MPD. Trying again shortly." );
    (void)g_timeout_add_seconds( 1, reconnect_mpd, data );
    return FALSE;
  }

  if ( mpd_changed( main_data->mpd, MPD_CHANGED_ANY ) ) {
    if ( display_update( main_data->mpd ) < 0 ) {
    }
  }

  return TRUE;
}
