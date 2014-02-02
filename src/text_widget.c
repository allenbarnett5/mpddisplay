/*
 * A text widget which provides some layout services and font
 * selection.
 */
#include <stdlib.h>

#include "pango/pangoft2.h"
#include "VG/openvg.h"

#include "text_widget.h"

static float float_from_26_6( FT_Pos x )
{
   return (float)x / 64.0f;
}

struct VG_DATA {
  VGFont font;
  uint8_t cmap[144]; // Probably should have an expandable structure. But
		     // we know we don't have any glyphs > 144*8 = 1152.
		     // Your collection may vary.
};

struct VG_DATA* vg_data_new ( void )
{
  struct VG_DATA* vg_data = malloc( sizeof( struct VG_DATA ) );
  vg_data->font = vgCreateFont( 256 );
  memset( vg_data->cmap, 0, sizeof vg_data->cmap );
  return vg_data;
}

void vg_data_free ( void* vg_data_ptr )
{
  vgDestroyFont( ((struct VG_DATA*)vg_data_ptr)->font );
}

static void add_char ( VGFont font, FT_Face face, FT_ULong c );

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
  if ( handle.d == NULL || handle.d->layout == NULL )
    return;

  pango_layout_set_markup( handle.d->layout, text->str, text->len );

  // The idea here is to make sure that the VGFont contains
  // all the glyphs at the proper size so that when we want
  // to draw, we can can just call vgDrawGlyphs (well, maybe
  // not since PangoGlyphInfo is not an array of glyph
  // indexes; aww).
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
    if ( face != NULL ) {
      struct VG_DATA* vg_data = face->size->generic.data;
      if ( vg_data == NULL ) {
	vg_data = vg_data_new();
	face->size->generic.data = vg_data;
	face->size->generic.finalizer = vg_data_free;
      }
      int g;
      for ( g = 0; g < run->glyphs->num_glyphs; g++ ) {
	int byte = run->glyphs->glyphs[g].glyph / 8;
	int bit  = 1 << ( run->glyphs->glyphs[g].glyph & 0x7 );
	if ( ! ( vg_data->cmap[byte] & bit  ) ) {
	  vg_data->cmap[byte] |= bit;
	  add_char( vg_data->font, face, run->glyphs->glyphs[g].glyph );
	}
      }
      pango_fc_font_unlock_face( (PangoFcFont*)font );
    }
  } while ( pango_layout_iter_next_run( li ) );

  pango_layout_iter_free( li );
}

void text_widget_draw_text ( struct TEXT_WIDGET_HANDLE handle )
{
  if ( handle.d == NULL || handle.d->layout == NULL )
    return;

  int height = PANGO_PIXELS( pango_layout_get_height( handle.d->layout ) );

  PangoLayoutIter* li = pango_layout_get_iter( handle.d->layout );
  do {
    PangoLayoutRun* run = pango_layout_iter_get_run( li );
    if ( run == NULL )
      continue;

    PangoRectangle logical_rect;
    int baseline_pango = pango_layout_iter_get_baseline( li );
    int baseline_pixel = PANGO_PIXELS( baseline_pango );
    pango_layout_iter_get_run_extents( li, NULL, &logical_rect );
    int x_pixel = PANGO_PIXELS( logical_rect.x );

    PangoFont* pg_font = run->item->analysis.font;

    FT_Face face = pango_fc_font_lock_face( (PangoFcFont*)pg_font );

    if ( face != NULL ) {
      struct VG_DATA* vg_data = face->size->generic.data;
      if ( vg_data != NULL ) {
	// Note: inverted Y coordinate
	VGfloat point[2] = { x_pixel, height-baseline_pixel };
	vgSetfv( VG_GLYPH_ORIGIN, 2, point );
	VGFont vg_font = vg_data->font;
	int g;
	for ( g = 0; g < run->glyphs->num_glyphs; g++ ) {
	  vgDrawGlyph( vg_font, run->glyphs->glyphs[g].glyph, VG_FILL_PATH,
		       VG_TRUE );
	}
      }
      pango_fc_font_unlock_face( (PangoFcFont*)pg_font );
    }
  } while ( pango_layout_iter_next_run( li ) );
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

// \bug this really needs help, too.

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
  // Pango already provides us with the font index, not the glyph UNICODE
  // point.

  FT_Load_Glyph( face, c, FT_LOAD_DEFAULT );

  FT_Outline *outline = &face->glyph->outline;

  VGPath path;
  path = vgCreatePath( VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
		       1.f, 0.f, 0, 0, VG_PATH_CAPABILITY_ALL );
  // It could be a blank. If any character doesn't have a glyph, though,
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

