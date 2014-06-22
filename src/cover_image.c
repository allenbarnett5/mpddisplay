/*
 * Find a cover image for the given album cover. I guess we can dig
 * around the internet looking for these things, but I will probably
 * just populate my database by hand.
 */
#include <stdio.h>
#include <stdlib.h>

#include "sqlite3.h"

#include "magick_image.h"
#include "log_intf.h"
#include "cover_image.h"

// The unknown cover is compiled into the code.
extern const unsigned char _binary_no_cover_png_start;
extern const unsigned char _binary_no_cover_png_end;

// If we can't get a cover image for some reason, use this one.
struct IMAGE_HANDLE no_cover ( void )
{
  // If we reach here, we haven't found an image.
  size_t no_cover_size =
    &_binary_no_cover_png_end - &_binary_no_cover_png_start; 

  return image_rgba_create( "PNG", &_binary_no_cover_png_start, no_cover_size );
}

/*!
 * The details of the database connection.
 */
struct IMAGE_DB_PRIVATE {
  sqlite3* db;
  struct LOG_HANDLE logger;
};

struct IMAGE_DB_HANDLE image_db_create ( const char* database,
					 struct LOG_HANDLE logger )
{
  int rc;
  struct IMAGE_DB_HANDLE handle;
  handle.d = malloc( sizeof( struct IMAGE_DB_PRIVATE ) );
  handle.d->logger = logger;

  rc = sqlite3_open_v2( database, &handle.d->db, SQLITE_OPEN_READONLY, 0 );

  if ( rc != SQLITE_OK ) {
    // Interesting: You still get a database handle even if the open
    // fails just so you can emit an error message! Though I must
    // say it is not very informative.
    log_message_error( logger, "Image database '%s': %s: (%d)",
		       database, sqlite3_errmsg( handle.d->db ), rc );
    sqlite3_close( handle.d->db );
    handle.d->db = NULL;
  }

  return handle;
}

struct IMAGE_HANDLE cover_image ( struct IMAGE_DB_HANDLE handle,
				  const char* artist, const char* album )
{
  if ( handle.d == NULL || handle.d->db == NULL ) {
    return no_cover();
  }

  int rc;
  sqlite3_stmt* stmt;
  const char* tail;

  rc = sqlite3_prepare_v2( handle.d->db, "SELECT albums.cover_format, albums.cover_image FROM contributions JOIN artists ON artists.ROWID = contributions.artist JOIN albums ON albums.ROWID = contributions.album WHERE artists.name = ? AND albums.title = ?", -1, &stmt, &tail );

  if ( rc != SQLITE_OK ) {
    log_message_warn( handle.d->logger,
		      "SQLITE3 error on prepare: %s\n",
		      sqlite3_errmsg(handle.d->db) );
    return no_cover();
  }

  rc = sqlite3_bind_text( stmt, 1, artist, -1, SQLITE_STATIC );
  rc = sqlite3_bind_text( stmt, 2, album, -1, SQLITE_STATIC );

  if ( rc == SQLITE_OK ) {
    while ( sqlite3_step( stmt ) == SQLITE_ROW ) {
      const char* format;
      const unsigned char* image;
      size_t n_bytes;
      format = (const char*)sqlite3_column_text( stmt, 0 );
      n_bytes = sqlite3_column_bytes( stmt, 1 );
      image = sqlite3_column_blob( stmt, 1 );
      if ( n_bytes == 0 )
	break;
      return image_rgba_create( format, image, n_bytes );
    }
  }

  return no_cover();
}

void image_db_free ( struct IMAGE_DB_HANDLE handle )
{
  if ( handle.d != NULL ) {
    if ( handle.d->db != NULL ) {
      sqlite3_close( handle.d->db );
    }
    free( handle.d );
    handle.d = NULL;
  }
}
