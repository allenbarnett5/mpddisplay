/*
 * Find a cover image for the given album cover. I guess we can dig
 * around the internet looking for these things, but I will probably
 * just populate my database by hand.
 */
#include <stdio.h>

#include "sqlite3.h"

#include "cover_image.h"

// The unknown cover is compiled into the code.
extern const unsigned char _binary_no_cover_png_start;
extern const unsigned char _binary_no_cover_png_end;

struct IMAGE_HANDLE cover_image ( const char* artist, const char* album )
{
  sqlite3* db;
  int rc;

  rc = sqlite3_open_v2( "album_art.sqlite3", &db, SQLITE_OPEN_READONLY, 0 );

  if ( rc == SQLITE_OK ) {
    sqlite3_stmt* stmt;
    const char* tail;

    rc = sqlite3_prepare_v2( db, "SELECT albums.cover_format, albums.cover_image FROM contributions JOIN artists ON artists.ROWID = contributions.artist JOIN albums ON albums.ROWID = contributions.album WHERE artists.name = ? AND albums.title = ?", -1, &stmt, &tail );

    if ( rc != SQLITE_OK ) {
      printf( "Hmm. SQLITE3 error on prepare: %s\n", sqlite3_errmsg(db) );
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
  }

  sqlite3_close( db );

  // If we reach here, we haven't found an image.
  size_t no_cover_size =
    &_binary_no_cover_png_end - &_binary_no_cover_png_start; 

  return image_rgba_create( "PNG", &_binary_no_cover_png_start, no_cover_size );
}
