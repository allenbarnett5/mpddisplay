#!/usr/bin/python

# Check with an MPD server and print the current song on the console.
# Has a fancy curses display which shows the current song, the time
# remaining for the current song.

# $Id$

from mpd import MPDClient, MPDError, CommandError
from datetime import timedelta
import curses

class PollerError ( Exception ):
    """Fatal error in poller."""

class MPDPoller ( object ):
    def __init__ ( self, stdscr, host="localhost", port="6600", password=None ):
        self._stdscr = stdscr
        self._host = host
        self._port = port
        self._password = password
        self._client = MPDClient()
        self._current = {}
        self._status = {}
        self._time_line = 0

        self.max_y, self.max_x = self._stdscr.getmaxyx()
        self._songwin = curses.newwin( self.max_y-2, self.max_x-2, 1, 1 )


    def connect ( self ):
        try:
            self._client.connect( self._host, self._port )
        # Catch socket exceptions.
        except IOError as ( errno, strerror ):
            raise PollerError( "Could not connect to '%s': %s" %
                               ( self._host, strerror ) )
        # Catch all other possible errors.  ConnectionError and
        # ProtocolError are always fatal. Others may not be, but we
        # don't know how to handle them here, so, treat them as if
        # they are (fatal) instead of ignoring them.
        except MPDError as e:
            raise PollerError( "Could not connect to '%s': %s" %
                               ( self._host, e ) )

        if self._password:
            try:
                self._client.password( self._password )
            # Catch errors with the password command (e.g., wrong password)
            except CommandError as e:
                raise PollerError( "Could not connect to '%s': "
                                   "password command failed: %s" %
                                   ( self._host, e ) )
            # Catch all other possible errors
            except ( MPDError, IOError ) as e:
                raise PollerError( "Could not connect to '%s': "
                                   "error with password command: %s" %
                                   ( self._host, e ) )

    def disconnect ( self ):
        # Try to tell MPD we're closing the connection first.
        try:
            self._client.close()
        except ( MPDError, IOError ):
            pass
        try:
            self._client.disconnect()
        # Disconnecting failed, so use a new client object instead.
        # This should never happen. If it does, something is seriously broken
        # and the client shouldn't be trusted to be reused.
        except ( MPDError, IOError ):
            self._client = MPDClient()

    def poll ( self ):
        try:
            song = self._client.currentsong()
            status = self._client.status()
        # Couldn't get the current song, so try reconnecting and retrying.
        except ( MPDError, IOError ):
            # No error handling required here. Our disconnect function
            # catches all exceptions, and therefore should never raise
            # any.
            self.disconnect()
            try:
                self.connect()
            # Reconnecting failed.
            except PollerError as e:
                raise PollerError( "Reconnecting failed: %s" % e )
            try:
                song = self._client.currentsong()
                time = self._client.status()['time']
            # Failed again, just give up
            except ( MPDError, IOError ) as e:
                raise PollerError( "Couldn't retrieve current song: %s" % e )
        # Hurray! We got the current song without any errors.
        if song != self._current:
            self._songwin.clear()
            song_str = ''
            if ( 'artist' in song ):
                song_str += song['artist']
            if ( 'album' in song ):
                song_str += '\n' + song['album']
            if ( 'title' in song ):
                song_str += '\n' + song['title']
            self._songwin.addstr( song_str )
            self._time_line = min( self._songwin.getyx()[0]+1, self.max_y-3 )
            self._songwin.refresh()
            self._current = song

        if status != self._status:
            if ( 'time' in status ):
                time = status['time']
                ( elapsed_time, run_time ) = time.split(':')
                etimed = timedelta( 0, int(elapsed_time) )
                rtimed = timedelta( 0, int(run_time) )
                time_str = str(etimed) + " / " + str(rtimed)
                self._songwin.addstr( self._time_line, 0, time_str )
                self._songwin.refresh()
                self._status = status

def main_loop( stdscr ):
    from time import sleep

    curses.curs_set(0)
    stdscr.border(0)
    stdscr.refresh()

    poller = MPDPoller( stdscr, 'guanaco' )
    poller.connect()
    while True:
        poller.poll()
        sleep(1)

def main():
    curses.wrapper( main_loop )

if __name__ == "__main__":
    from sys import stderr
    from time import sleep
    # This really needs some improvement. But we can't really tell why
    # we've lost the connection to MPD. Do we really want to run on
    # forever? We should at least replace the display with a note that
    # we've lost contact with the plenum.
    while True:
        try:
            main()
        # Catch fatal poller errors.
        except PollerError as e:
            print >> stderr, "Fatal poller error: %s" % e
        # Catch all other non-exit errors
        except Exception as e:
            print >> stderr, "Unexpected exception: %s" % e
        # Catch remaining exit errors
        except:
            pass
        # Don't poll too quickly.
        sleep(5)
