/*
 * Encapsulate the interaction with the Pi's display.
 */
#ifndef DISPLAY_INTF_H
#define DISPLAY_INTF_H

struct MPD_HANDLE;

/*!
 * Initialize the display.
 * \return 0 if everything went ok, -1 otherwise.
 */
int display_init ( void );
/*!
 * Update the display using the MPD information.
 * \param[in] handle the MPD object.
 * \return 0 if everything went ok, -1 otherwise.
 */
int display_update ( const struct MPD_HANDLE handle );
/*!
 * Restore the display to whatever it showed before.
 * \return 0 if everything went ok, -1 otherwise.
 */
int display_close ( void );
#endif
