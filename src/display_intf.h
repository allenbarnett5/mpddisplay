/*
 * Encapsulate the interaction with the Pi's display.
 */
#ifndef DISPLAY_INTF_H
#define DISPLAY_INTF_H
#if 0
struct MPD_CURRENT;
#endif
struct MPD_HANDLE;

/*!
 * Initialize the display.
 * \return 0 if everything went ok, -1 otherwise.
 */
int display_init ( void );
#if 0
/*!
 * Update the display using the MPD information.
 * \param current the most recent data returned from MPD.
 * \return 0 if everything went ok, -1 otherwise.
 */
int display_update ( const struct MPD_CURRENT* current );
#endif
/*!
 * Update the display using the MPD information.
 * \param[in] handle the MPD object.
 * \return 0 if everything went ok, -1 otherwise.
 */
int display_update2 ( const struct MPD_HANDLE handle );
/*!
 * Restore the display to whatever it showed before.
 * \return 0 if everything went ok, -1 otherwise.
 */
int display_close ( void );
#endif
