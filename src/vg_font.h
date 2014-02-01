/*
 * Yet another font handling class. Why? Well, I need UNICODE.
 */
#ifndef VG_FONT_H
#define VG_FONT_H

#include "glib.h"

struct VG_FONT_PRIVATE;

struct VG_FONT_HANDLE {
  struct VG_FONT_PRIVATE* d;
};

/*!
 * Create a font object.
 * * It needs to be able to load a font file (via FreeType2 of course).
 * * Keep track of which glyphs have been used.
 * * Compute string lengths for wrapping (maybe provide the wrapping
 * algorithm itself)
 * \param font_file the path to a font file.
 * \param size the height of the font in pt (or mm?)
 * \param dpi_x resolution of the screen in the x direction.
 * \param dpi_y resolution of the screen in the y direction.
 */
struct VG_FONT_HANDLE* vg_font_init ( const char* font_file,
				      float size, float dpi_x, float dpi_y );
/*!
 * \return the line spacing for the font (at the current size).
 */
float vg_font_line_height ( struct VG_FONT_HANDLE* font );

/*!
 * Draw the string.
 * \param font font info.
 * \param x x position.
 * \param y y position.
 * \param string the string.
 */
void vg_font_draw_string ( struct VG_FONT_HANDLE* font,
			   float x, float y,
			   const GString* string );
			   
/*!
 * Release any memory held by the handle.
 */
void vg_font_free_handle ( struct VG_FONT_HANDLE* font );
#endif
