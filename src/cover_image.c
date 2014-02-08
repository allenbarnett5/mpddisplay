/*
 * Find a cover image for the given album cover. I guess we can dig
 * around the internet looking for these things, but I will probably
 * just populate my database by hand.
 */

#include "cover_image.h"

// The unknown cover is compiled into the code.
extern const char _binary_no_cover_png_start;
extern const char _binary_no_cover_png_end;

struct IMAGE_HANDLE cover_image ( const char* artist, const char* album )
{
  size_t no_cover_size =
    &_binary_no_cover_png_end - &_binary_no_cover_png_start; 

  return image_rgba_create( "PNG", &_binary_no_cover_png_start, no_cover_size );
}
