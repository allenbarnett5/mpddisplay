/*
 * Show a single character or draw a box or something.
 */
#pragma once

enum EMBLEM_WIDGET_EMBLEM {
  EMBLEM_WIDGET_EMBLEM_STOPPED,
  EMBLEM_WIDGET_EMBLEM_PLAYING,
  EMBLEM_WIDGET_EMBLEM_PAUSED,
  EMBLEM_WIDGET_EMBLEM_NOEMBLEM
};

struct EMBLEM_WIDGET_PRIVATE;

struct EMBLEM_WIDGET_HANDLE {
  struct EMBLEM_WIDGET_PRIVATE* d;
};

/*!
 * Create an emblem widget.
 * \param[in] x_mm position of the left edge in mm.
 * \param[in] y_mm position of the bottom edge in mm.
 * \param[in] width_mm width of the window in mm.
 * \param[in] height_mm height of the window in mm.
 * \param[in] screen_width_mm width of screen in mm.
 * \param[in] screen_height_mm height of screen in mm.
 */
struct EMBLEM_WIDGET_HANDLE emblem_widget_init ( float x_mm, float y_mm,
                                                 float width_mm,
                                                 float height_mm,
                                                 float screen_width_mm,
                                                 float screen_height_mm );

/*!
 * Designate the current emblem.
 */
void emblem_widget_set_emblem ( struct EMBLEM_WIDGET_HANDLE handle,
                                enum EMBLEM_WIDGET_EMBLEM emblem );
/*!
 * Draw the emble widget.
 */
void emblem_widget_draw_emblem ( struct EMBLEM_WIDGET_HANDLE handle );
