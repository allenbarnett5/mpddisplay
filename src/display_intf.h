/*
 * Encapsulate the interaction with the Pi's display.
 */
#ifndef DISPLAY_INTF_H
#define DISPLAY_INTF_H

struct MPD_HANDLE;
struct IMAGE_DB_HANDLE;
struct LOG_HANDLE;

struct DISLPAY_PRIVATE;

struct DISPLAY_HANDLE {
  struct DISPLAY_PRIVATE* d;
};

struct TOUCH_EVENT {
  int updown;
  int x;
  int y;
};

/*!
 * Initialize the display.
 * \param[in] image_db image database connector.
 * \param[in] mpd connection to MPD.
 * \return a handle to the display.
 */
struct DISPLAY_HANDLE display_init ( struct IMAGE_DB_HANDLE image_db,
				     struct MPD_HANDLE mpd,
                                     struct LOG_HANDLE logger );
/*!
 * The structure is opaque so every access has to be through
 * a function call.
 * \return 0 if everything is ok, -1 otherwise.
 */
int display_status ( struct DISPLAY_HANDLE handle );
/*!
 * Update the display.
 * \param[in] handle our display.
 */
void display_update ( struct DISPLAY_HANDLE handle );
/*!
 * Restore the display to whatever it showed before.
 * \param[in] handle the display handle to close.
 */
void display_close ( struct DISPLAY_HANDLE handle  );
/*!
 * Do something with a touch event.
 * \param[in] handle our display.
 */
void display_dispatch_touch ( struct DISPLAY_HANDLE handle, 
                              const struct TOUCH_EVENT* touch );
#endif
