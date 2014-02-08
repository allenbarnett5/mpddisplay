/*
 * Get an album cover image.
 */
#ifndef COVER_IMAGE_H
#define COVER_IMAGE_H

#include "magick_image.h"

struct IMAGE_HANDLE cover_image ( const char* artist,
				  const char* album );

#endif
