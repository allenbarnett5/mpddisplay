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

static void add_pibrella_button10 ( gpointer data );
static void add_pibrella_button11 ( gpointer data );

const char* USAGE = "usage: %s [--host hostname] [--port port#] [--database databse]\n";

struct MAIN_DATA {
  struct DISPLAY_HANDLE display;
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
#if 0
  char* port = "6600";    // The standard MPD port.
#else
  int   port = 6600;    // The standard MPD port.
#endif
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
#if 0
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
#else
      {
	port = convert_int( optarg );
	if ( errno != 0 ) {
	  bad_argument = true;
	  printf( "--port argument was not a valid integer: '%s' (%s)\n",
		  optarg, strerror( errno ) );
	}
	if ( port <= 0 ) {
	  bad_argument = true;
	  printf( "--port argument must be positive (%d)\n", port );
	}
      }
#endif
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
  log_message_info( main_data.logger, "MPD port: '%d'", port );
  log_message_info( main_data.logger, "Database: '%s'", database );

  main_data.mpd = mpd_create( host, port, main_data.logger );

  if ( mpd_status( main_data.mpd ) < 0 ) {
    printf( USAGE, argv[0] );
    return 1;
  }

  // Try to open the image database connection.

  main_data.image_db = image_db_create( database, main_data.logger );

  // If we get this far, we can try to initialize the graphics.

  main_data.display = display_init( main_data.image_db, main_data.mpd );

  if ( display_status( main_data.display ) < 0 ) {
    return 1;
  }

  // Well, after all that, we can now start polling MPD to see what's up.

  main_data.loop = g_main_loop_new( NULL, FALSE );

  // Poll MPD periodically.
  (void)g_timeout_add_seconds( 1, poll_mpd, &main_data );

  // Also try to poll the buttons on the Pibrella (if it is configured
  // properly).
  add_pibrella_button10( &main_data );
  add_pibrella_button11( &main_data );

  g_main_loop_run( main_data.loop );

  log_message_info( main_data.logger, "Done with main loop. Cleaning up." );

  g_main_loop_unref( main_data.loop );

  display_close( main_data.display );

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
    display_update( main_data->display );
    // \bug maybe check for error...
  }

  return TRUE;
}

gboolean play_pause_button_callback ( GIOChannel* gio,
				      GIOCondition condition,
				      gpointer data )
{
  // Don't bother with anything if it's not the interrupt.
  if ( condition != G_IO_PRI ) {
    return TRUE;
  }

  struct LOG_HANDLE logger = ((struct MAIN_DATA*)data)->logger;
  GIOStatus status;
  gchar byte;
  gsize bytes_read;
  GError* error = NULL;

  log_message_info( logger, "Called the button callback" );

  status = g_io_channel_seek_position( gio, 0, G_SEEK_SET, &error );

  if ( status == G_IO_STATUS_ERROR ) {
    log_message_warn( logger, "Error seeking button: %s", error->message );

    g_error_free( error );
    return TRUE;
  }

  error = NULL;
  // OK. But the button really needs to be debounced...
  status = g_io_channel_read_chars( gio, &byte, 1, &bytes_read, &error );

  if ( status == G_IO_STATUS_ERROR ) {
    log_message_warn( logger, "Error reading button: %s", error->message );

    g_error_free( error );
  }
  else if ( bytes_read == 0 ) {
    log_message_warn( logger, "Read button OK but no data" );
  }
  else {
    if ( byte == '1' ) {
      struct MPD_HANDLE mpd = ((struct MAIN_DATA*)data)->mpd;
      mpd_play_pause( mpd );
    }
  }

  return TRUE;
}

gboolean exit_button_callback ( GIOChannel* gio,
				GIOCondition condition,
				gpointer data )
{
  // Don't bother with anything if it's not the interrupt.
  if ( condition != G_IO_PRI ) {
    return TRUE;
  }

  struct LOG_HANDLE logger = ((struct MAIN_DATA*)data)->logger;
  GIOStatus status;
  gchar byte;
  gsize bytes_read;
  GError* error = NULL;

  log_message_info( logger, "Called the button callback" );

  status = g_io_channel_seek_position( gio, 0, G_SEEK_SET, &error );

  if ( status == G_IO_STATUS_ERROR ) {
    log_message_warn( logger, "Error seeking button: %s", error->message );

    g_error_free( error );
    return TRUE;
  }

  error = NULL;
  // OK. But the button really needs to be debounced...
  status = g_io_channel_read_chars( gio, &byte, 1, &bytes_read, &error );

  if ( status == G_IO_STATUS_ERROR ) {
    log_message_warn( logger, "Error reading button: %s", error->message );

    g_error_free( error );
  }
  else if ( bytes_read == 0 ) {
    log_message_warn( logger, "Read button OK but no data" );
  }
  else {
    if ( byte == '1' ) {
      // Use the button to exit!
      log_message_warn( logger, "Button: Exiting main loop" );
      g_main_loop_quit( ((struct MAIN_DATA*)data)->loop );
    }
  }

  return TRUE;
}

void add_pibrella_button11 ( gpointer data )
{
  struct MAIN_DATA* main_data = data;
  struct LOG_HANDLE logger = main_data->logger;

  // We want the interrupt mode. But /sys/class/gpio operations
  // are usually privileged. So, we call out to the "gpio" program.
  gchar*  stdout_buffer = NULL;
  gchar*  stderr_buffer = NULL;
  gint    exit_status   = 0;
  GError* error         = NULL;
  gboolean result =
    g_spawn_command_line_sync( "gpio edge 11 rising",
			       &stdout_buffer, &stderr_buffer, &exit_status,
			       &error );

  if ( ! result ) {
    log_message_warn( logger, "Could not set gpio 11 button to rising interrupt: %s",
		      error->message );
    g_error_free( error );
    return;
  }
  else {
    g_free( stdout_buffer );
    g_free( stderr_buffer );
  }

  GIOChannel* button;

  guint ret;
  char* gpio_file = "/sys/class/gpio/gpio11/value";
  error = NULL;

  // Well, it goes without saying that the GPIO pin should be an
  // argument to the program.
  button = g_io_channel_new_file( gpio_file, "r", &error );

  if ( ! button ) {
    log_message_warn( logger, "Could not open GPIO file \"%s\": %s",
		      gpio_file, error->message );
    g_error_free( error );
    return;
  }
  else {
    log_message_info( logger, "Opened GPIO \"%s\"", gpio_file );
  }

  ret = g_io_add_watch( button, G_IO_PRI, play_pause_button_callback, data );

  if ( ! ret ) {
    log_message_warn( logger, "Error creating watch on button" );
    return;
  }
}

void add_pibrella_button10 ( gpointer data )
{
  struct MAIN_DATA* main_data = data;
  struct LOG_HANDLE logger = main_data->logger;

  // We want the interrupt mode. But /sys/class/gpio operations
  // are usually privileged. So, we call out to the "gpio" program.
  gchar*  stdout_buffer = NULL;
  gchar*  stderr_buffer = NULL;
  gint    exit_status   = 0;
  GError* error         = NULL;
  gboolean result =
    g_spawn_command_line_sync( "gpio edge 10 rising",
			       &stdout_buffer, &stderr_buffer, &exit_status,
			       &error );

  if ( ! result ) {
    log_message_warn( logger, "Could not set gpio 10 button to rising interrupt: %s",
		      error->message );
    g_error_free( error );
    return;
  }
  else {
    g_free( stdout_buffer );
    g_free( stderr_buffer );
  }

  GIOChannel* button;

  guint ret;
  char* gpio_file = "/sys/class/gpio/gpio10/value";
  error = NULL;

  // Well, it goes without saying that the GPIO pin should be an
  // argument to the program.
  button = g_io_channel_new_file( gpio_file, "r", &error );

  if ( ! button ) {
    log_message_warn( logger, "Could not open GPIO file \"%s\": %s",
		      gpio_file, error->message );
    g_error_free( error );
    return;
  }
  else {
    log_message_info( logger, "Opened GPIO \"%s\"", gpio_file );
  }

  ret = g_io_add_watch( button, G_IO_PRI, exit_button_callback, data );

  if ( ! ret ) {
    log_message_warn( logger, "Error creating watch on button" );
    return;
  }
}
