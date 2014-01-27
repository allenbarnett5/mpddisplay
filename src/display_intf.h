/*
 * Encapsulate the interaction with the Pi's display.
 */
#ifndef DISPLAY_INTF_H
#define DISPLAY_INTF_H
/*!
 * Initialize the display.
 * \return 0 if everything went ok, -1 otherwise.
 */
int display_init ( void );
/*!
 * Restore the display to whatever it showed before.
 * \return 0 if everything went ok, -1 otherwise.
 */
int display_close ( void );
#endif
