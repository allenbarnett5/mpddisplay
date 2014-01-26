/*
 * Encapsulate the interaction with MPD. Because this is a network
 * application, there is a lot which can go wrong.
 */
#ifndef MPD_INTF_H
#define MPD_INTF_H

#include "firestring.h"
/*!
 * Simple status of MPD.
 */
enum MPD_PLAY_STATUS {
  MPD_PLAY_STATUS_NOSONG,
  MPD_PLAY_STATUS_STOPPED,
  MPD_PLAY_STATUS_PLAYING,
  MPD_PLAY_STATUS_PAUSED
};
/*!
 * Status changed bits. Or'd together.
 */
enum MPD_CHANGED {
  MPD_CHANGED_ARTIST  = 0x01,
  MPD_CHANGED_ALBUM   = 0x02,
  MPD_CHANGED_TITLE   = 0x04,
  MPD_CHANGED_ELAPSED = 0x08,
  MPD_CHANGED_TOTAL   = 0x10,
  MPD_CHANGED_STATUS  = 0x20,
};
/*!
 * This is the structure which is populated by mpd_get_current method.
 * changed notes of any fields are different from the last time
 * mpd_get_current was called.
 */
struct MPD_CURRENT {
  //! Bitmap of changed values.
  int changed;
  //! Play status.
  enum MPD_PLAY_STATUS play_status;
  //! The artist (UTF-8)
  struct firestring_estr_t artist;
  //! The album (UTF-8)
  struct firestring_estr_t album;
  //! The title (UTF-8)
  struct firestring_estr_t title;
  //! Elapsed time in seconds.
  int elapsed_time;
  //! Total track time in seconds.
  int total_time;
};


/*!
 * Connect to the music player daemon on the given host at the
 * given port.
 * \param[in] host the host name.
 * \param[in] port the port (well, really this is the "service" passed
 * to getaddrinfo()).
 * \return socket fd if everything went OK. If less than zero,
 * then something went wrong.
 */
int mpd_connect ( const char* host, const char* port );
/*!
 * Some initialization is required.
 */
void mpd_current_init ( struct MPD_CURRENT* current );
/*!
 * Retrieve information about the current state of the player. previous
 * is updated with the current information.
 * \param[in] mfd socket fd for connection.
 * \param[inout] previous the previous status.
 * \return zero if everything went OK.
 */
int mpd_get_current ( int mfd, struct MPD_CURRENT* previous );
/*!
 * And some un-initialization is required.
 */
void mpd_current_free ( struct MPD_CURRENT* current );
/*!
 * Ordinarily, this program will run forever, so hopefully this is
 * never used. But, we might drop the network connection or some
 * such
 */
void mpd_close ( int mfd );
#endif
