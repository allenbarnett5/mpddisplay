/*
 * Get an album cover image from a database.
 */
#ifndef COVER_IMAGE_H
#define COVER_IMAGE_H

struct IMAGE_HANDLE;
struct IMAGE_DB_PRIVATE;
struct LOG_HANDLE;

struct IMAGE_DB_HANDLE {
  struct IMAGE_DB_PRIVATE* d;
};

struct IMAGE_DB_HANDLE image_db_create ( const char* database,
					 struct LOG_HANDLE );

struct IMAGE_HANDLE cover_image ( struct IMAGE_DB_HANDLE handle,
				  const char* artist,
				  const char* album );

void image_db_free ( struct IMAGE_DB_HANDLE handle );
#endif
