/*
 * A text widget which provides some layout services and font
 * selection.
 */
#include <stdlib.h>
#include "pango/pangoft2.h"
#if 0
#include <stdint.h>
#include <errno.h>
#include <iconv.h>
#include "ft2build.h"
#include FT_FREETYPE_H
#include "VG/openvg.h"
#include "VG/vgu.h"
#endif

#include "text_widget.h"

static float float_from_26_6( FT_Pos x )
{
   return (float)x / 64.0f;
}

struct TEXT_WIDGET_PRIVATE {
  PangoFontMap* font_map;
  PangoContext* context;
  PangoLayout* layout;
};

struct TEXT_WIDGET_HANDLE text_widget_init ( float width_mm,
					     float height_mm,
					     int width_pixels,
					     int height_pixels )
{
  struct TEXT_WIDGET_HANDLE handle;
  handle.d = malloc( sizeof( struct TEXT_WIDGET_PRIVATE ) );
  handle.d->font_map = pango_ft2_font_map_new();

  pango_ft2_font_map_set_resolution( (PangoFT2FontMap*)handle.d->font_map,
			     width_pixels / ( (double)width_mm / 25.4 ),
			     height_pixels / ( (double)height_mm / 25.4 ) );

  handle.d->context  = pango_font_map_create_context( handle.d->font_map );
  handle.d->layout   = pango_layout_new( handle.d->context );

  pango_layout_set_width( handle.d->layout,
			  pango_units_from_double( width_pixels ) );
  pango_layout_set_height( handle.d->layout,
			   pango_units_from_double( height_pixels ) );

  return handle;
}

void text_widget_set_text ( struct TEXT_WIDGET_HANDLE handle,
			    const GString* text )
{
  // This function is trivial now!
  if ( handle.d == NULL || handle.d->layout == NULL )
    return;

  pango_layout_set_markup( handle.d->layout, text->str, text->len );
}

void text_widget_draw_text ( struct TEXT_WIDGET_HANDLE handle )
{
  // This function is not trivial, though.
  if ( handle.d == NULL || handle.d->layout == NULL )
    return;
  printf( "<layout start>\n" );
  PangoLayoutIter* li = pango_layout_get_iter( handle.d->layout );
  do {
    PangoLayoutRun* run = pango_layout_iter_get_run( li );
    if ( run == NULL )
      continue;
    PangoFont* font = run->item->analysis.font;
    // Well, you can see how C is not the most ideal language for
    // abstraction. Have to read the documentation to discover
    // that this font is a PangoFcFont.
    FT_Face face = pango_fc_font_lock_face( (PangoFcFont*)font );
    printf( "face is %p: ", face );
    if ( face != NULL ) {
      printf( "height %ld[%f] (x %d,y %d)\n", face->size->metrics.height,
	      float_from_26_6( face->size->metrics.height ),
	      face->size->metrics.x_ppem, face->size->metrics.y_ppem );
    }
    else {
      printf( "hmm:-(\n" );
    }
  } while ( pango_layout_iter_next_run( li ) );

  pango_layout_iter_free( li );

  printf( "<layout end>\n" );
}

void text_widget_free_handle ( struct TEXT_WIDGET_HANDLE handle )
{
  if ( handle.d != 0 ) {
    g_object_unref( handle.d->layout );
    g_object_unref( handle.d->context );
    g_object_unref( handle.d->font_map );
    free( handle.d );
    handle.d = 0;
  }
}

#if 0
#include "vg_font.h"

static VGfloat float_from_26_6( FT_Pos x )
{
   return (VGfloat)x / 64.0f;
}

// This is the number of UNICODE "blocks" we expect to have to
// draw glyphs for. I've surveyed my audio files and there are
// no glyphs above U+07ff. So, this plus 256 bits per "block"
// should be enough without resorting to an ordered-set or
// hash table.
#define N_BLOCKS (8)
#define N_WORDS (256/(8*sizeof(uint32_t)))

const float PT_PER_MM = 72.f / 25.4f;

struct VG_FONT_PRIVATE {
  FT_Library library;
  FT_Face face;
  iconv_t utf8_utf32;
  uint32_t has_vg_glyph[N_BLOCKS][N_WORDS];
  VGFont font;
};

static void add_char ( VGFont font, FT_Face face, FT_ULong c );

struct VG_FONT_HANDLE* vg_font_init ( const char* font_file,
				      float size, float dpi_x, float dpi_y )
{
  struct VG_FONT_HANDLE* handle = malloc( sizeof( struct VG_FONT_HANDLE ) );

  if ( handle == NULL ) {
    printf( "Error: Out of memory for font handle\n" );
    return NULL;
  }

  handle->d = malloc( sizeof( struct VG_FONT_PRIVATE ) );

  if ( handle->d == NULL ) {
    printf( "Error: Out of memory for font handle private object\n" );
    free( handle );
    return NULL;
  }

  memset( handle->d->has_vg_glyph, 0, sizeof handle->d->has_vg_glyph );

  FT_Error error;

  error = FT_Init_FreeType( &handle->d->library );

  if ( error != 0 ) {
    printf( "Error: Could not initialize the FreeType library: %d\n", error );
    vg_font_free_handle( handle );
    return NULL;
  }

  error = FT_New_Face( handle->d->library, font_file, 0, &handle->d->face );

  if ( error != 0 ) {
    printf( "Error: Could not load the face from file: %s, error %d\n",
	    font_file, error );
    vg_font_free_handle( handle );
    return NULL;
  }

  FT_Set_Char_Size( handle->d->face, 0, (size * PT_PER_MM) * 64,
		    (FT_UInt)dpi_x, (FT_UInt)dpi_y );

  handle->d->utf8_utf32 = iconv_open( "UTF-32LE", "UTF-8" );

  if ( handle->d->utf8_utf32 == (iconv_t)-1) {
    printf( "Error: Could not create UTF-8 to UTF-32 converter: %s\n",
	    strerror( errno ) );
    // \bug close the freetype stuff, too.
    vg_font_free_handle( handle );
    return NULL;
  }

  handle->d->font = vgCreateFont( 128 );

  return handle;
}

float vg_font_line_height ( struct VG_FONT_HANDLE* handle )
{
  return float_from_26_6( handle->d->face->size->metrics.height );
}

void vg_font_draw_string ( struct VG_FONT_HANDLE* handle,
			   float x, float y,
			   const GString* string )
{
  char* in = string->str;
  size_t n_in = string->len;
  size_t utf32_size = sizeof(uint32_t) * n_in; // Guess this is enough.
  size_t n_out = utf32_size;
  uint32_t* utf32 = malloc( utf32_size );
  char* out = (char*)utf32;
  size_t n_conv;
  n_conv = iconv( handle->d->utf8_utf32, &in, &n_in, &out, &n_out );
  if ( n_conv == (size_t)-1 ) {
    // Well, if errno == E2BIG, we could try again with a bigger
    // buffer, otherwise this is a bit hopeless.
  }

  size_t n_char = ( utf32_size - n_out ) / 4;
  size_t c;
  for ( c = 0; c < n_char; c++ ) {
    int block = utf32[c] >> 8;
    int word  = utf32[c] & 0xff;
    if ( block < N_BLOCKS ) {
      int word_i = word / N_WORDS;
      int bit_i  = 1 << ( word & 0x7 );
      if ( handle->d->has_vg_glyph[block][word_i] & bit_i ) {
      }
      else {
	handle->d->has_vg_glyph[block][word_i] |= bit_i;
	add_char( handle->d->font, handle->d->face, utf32[c] );
      }
    }
    else {
      // Switch to undefined character?
    }
  }

  VGfloat point[2] = { x, y };
  vgSetfv( VG_GLYPH_ORIGIN, 2, point );
  vgDrawGlyphs( handle->d->font, n_char, utf32, 0, 0, VG_FILL_PATH, VG_TRUE );

  free( utf32 );
}

void vg_font_free_handle ( struct VG_FONT_HANDLE* handle )
{
  vgDestroyFont( handle->d->font );
  free( handle->d );
  free( handle );
}

#define SEGMENTS_COUNT_MAX 256
#define COORDS_COUNT_MAX 1024

static VGuint segments_count;
static VGubyte segments[SEGMENTS_COUNT_MAX];
static VGuint coords_count;
static VGfloat coords[COORDS_COUNT_MAX];


// \bug unless we replace these arrays with vectors, we must
// restore the assertions in some form.

static void convert_contour ( const FT_Vector *points,
			      const char *tags,
			      short points_count )
{
   int first_coords = coords_count;

   int first = 1;
   char last_tag = 0;
   int c = 0;

   for (; points_count != 0; ++points, ++tags, --points_count) {
      ++c;

      char tag = *tags;
      if (first) {
         /* assert(tag & 0x1); */
         /* assert(c==1); c=0; */
         segments[segments_count++] = VG_MOVE_TO;
         first = 0;
      } else if (tag & 0x1) {
         /* on curve */

         if (last_tag & 0x1) {
            /* last point was also on -- line */
            /* assert(c==1); c=0; */
            segments[segments_count++] = VG_LINE_TO;
         } else {
            /* last point was off -- quad or cubic */
            if (last_tag & 0x2) {
               /* cubic */
               /* assert(c==3); c=0; */
               segments[segments_count++] = VG_CUBIC_TO;
            } else {
               /* quad */
               /* assert(c==2); c=0; */
               segments[segments_count++] = VG_QUAD_TO;
            }
         }
      } else {
         /* off curve */

         if (tag & 0x2) {
            /* cubic */

            /* assert((last_tag & 0x1) || (last_tag & 0x2)); /\* last either on or off and cubic *\/ */
         } else {
            /* quad */

            if (!(last_tag & 0x1)) {
               /* last was also off curve */

               /* assert(!(last_tag & 0x2)); /\* must be quad *\/ */

               /* add on point half-way between */
               /* assert(c==2); c=1; */
               segments[segments_count++] = VG_QUAD_TO;
               VGfloat x = (coords[coords_count - 2] + float_from_26_6(points->x)) * 0.5f;
               VGfloat y = (coords[coords_count - 1] + float_from_26_6(points->y)) * 0.5f;
               coords[coords_count++] = x;
               coords[coords_count++] = y;
            }
         }
      }
      last_tag = tag;

      coords[coords_count++] = float_from_26_6(points->x);
      coords[coords_count++] = float_from_26_6(points->y);
   }

   if (last_tag & 0x1) {
      /* last point was also on -- line (implicit with close path) */
      /* assert(c==0); */
   } else {
      ++c;

      /* last point was off -- quad or cubic */
      if (last_tag & 0x2) {
         /* cubic */
         /* assert(c==3); c=0; */
         segments[segments_count++] = VG_CUBIC_TO;
      } else {
         /* quad */
         /* assert(c==2); c=0; */
         segments[segments_count++] = VG_QUAD_TO;
      }

      coords[coords_count++] = coords[first_coords + 0];
      coords[coords_count++] = coords[first_coords + 1];
   }

   segments[segments_count++] = VG_CLOSE_PATH;
}

static void convert_outline ( const FT_Vector *points,
			      const char *tags,
			      const short *contours,
			      short contours_count, short points_count )
{
   segments_count = 0;
   coords_count = 0;

   short last_contour = 0;
   for (; contours_count != 0; ++contours, --contours_count) {
      short contour = *contours + 1;
      convert_contour(points + last_contour, tags + last_contour, contour - last_contour);
      last_contour = contour;
   }
   /* assert(last_contour == points_count); */

   /* assert(segments_count <= SEGMENTS_COUNT_MAX); /\* oops... we overwrote some memory *\/ */
   /* assert(coords_count <= COORDS_COUNT_MAX); */
}

static void add_char ( VGFont font, FT_Face face, FT_ULong c )
{
  FT_UInt cindex = FT_Get_Char_Index( face, c );

  FT_Load_Glyph( face, cindex, FT_LOAD_DEFAULT );

  FT_Outline *outline = &face->glyph->outline;

  VGPath path;
  path = vgCreatePath( VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
		       1.f, 0.f, 0, 0, VG_PATH_CAPABILITY_ALL );
  // It could be a blank. If any character doesn't have a glyph,
  // nothing is drawn by vgDrawGlyphs.
  if ( outline->n_contours > 0 ) {
    convert_outline( outline->points, outline->tags, outline->contours,
		     outline->n_contours, outline->n_points );
    vgAppendPathData( path, segments_count, segments, coords );
  }

  VGfloat origin[] = { 0.f, 0.f };
  VGfloat escapement[] = { float_from_26_6(face->glyph->advance.x),
			   float_from_26_6(face->glyph->advance.y) };

  vgSetGlyphToPath( font, c, path, VG_TRUE, origin, escapement );

  vgDestroyPath( path );
}
#endif
