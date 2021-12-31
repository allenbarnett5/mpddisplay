/*
 * Display a thermometer.
 */
#pragma once

struct THERMOMETER_WIDGET_PRIVATE;

struct THERMOMETER_WIDGET_HANDLE {
  struct THERMOMETER_WIDGET_PRIVATE* d;
};

/*!
 * Create a thermometer widget.
 * \param[in] x_mm position of the left edge in mm.
 * \param[in] y_mm position of the bottom edge in mm.
 * \param[in] width_mm width of the window in mm.
 * \param[in] height_mm height of the window in mm.
 * \param[in] screen_width_mm width of screen in mm.
 * \param[in] screen_height_mm height of screen in mm.
 */
struct THERMOMETER_WIDGET_HANDLE thermometer_widget_init ( float x_mm, float y_mm,
					       float width_mm,
					       float height_mm,
					       float screen_width_mm,
					       float screen_height_mm );

/*!
 * Replace the thermometer.
 * \param[in] value new value. Between 0. and 100. I guess.
 */
void thermometer_widget_set_value ( struct THERMOMETER_WIDGET_HANDLE handle,
                                    float value );
/*!
 * Draw the thermometer widget.
 */
void thermometer_widget_draw_thermometer ( struct THERMOMETER_WIDGET_HANDLE handle );

/*!
 * Release any memory held by the handle.
 */
void thermometer_widget_free_handle ( struct THERMOMETER_WIDGET_HANDLE handle );
