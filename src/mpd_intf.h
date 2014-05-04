/*
 * Encapsulate the interaction with MPD. Because this is a network
 * application, there is a lot which can go wrong.
 */
#ifndef MPD_INTF_H
#define MPD_INTF_H

#include <stdbool.h>
#include <time.h>
#if 0
#include "glib.h"
#endif
struct MPD_PRIVATE;

struct MPD_HANDLE {
  struct MPD_PRIVATE* d;
};

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
  MPD_CHANGED_ANY     = 0xff, //!< Has anything changed?
};
/*!
 * Time structure.
 */
struct MPD_TIMES {
  time_t elapsed;
  time_t total;
};
#if 0
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
  GString* artist;
  //! The album (UTF-8)
  GString* album;
  //! The title (UTF-8)
  GString* title;
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
/*!
 * Connect to the music player daemon on the given host at the
 * given port.
 * \param[in] host the host name.
 * \param[in] port the port (well, really this is the "service" passed
 * to getaddrinfo()).
 * \return a handle to the MPD connection.
 */
struct MPD_HANDLE mpd_create ( const char* host, const char* port );
/*!
 * \param[in] handle MPD connection.
 * \return the status of the MPD connection. < 0 is bad.
 */
int mpd_status ( const struct MPD_HANDLE handle );
/*!
 * Release any resources held by the connection.
 * \param[in,out] handle MPD connection.
 */
void mpd_free ( struct MPD_HANDLE handle );
/*!
 * Poll the MPD daemon.
 * \parma[in,out] handle MPD connection.
 * \return the success of the action. Less than 0 means we lost
 * the connection to the server.
 */
int mpd_poll( struct MPD_HANDLE handle );
/*!
 * \param[in] handle MPD connection.
 * \param[in] flags or'd list of fields to query (or MPD_CHANGED_ANY).
 * \return true if the last poll showed that some aspect of the
 * state has changed.
 */
bool mpd_changed ( const struct MPD_HANDLE handle, int flags );
/*!
 * \return the current artist.
 */
char* mpd_artist ( const struct MPD_HANDLE handle );
/*!
 * \return the current album.
 */
char* mpd_album ( const struct MPD_HANDLE handle );
/*!
 * \return the current (song) title.
 */
char* mpd_title ( const struct MPD_HANDLE handle );
/*!
 * \return the time attributes of the current song.
 */
struct MPD_TIMES mpd_times ( const struct MPD_HANDLE handle );
#endif
