#!/usr/bin/python

# Build the initial database of album images.

from mpd import MPDClient
import sqlite3

player = MPDClient()
player.connect( 'guanaco', 6600 )

songs = player.listallinfo()
artists = set()
albums = set()
catalog = set()

for song in songs:
    artist = 'unknown'
    album = 'unknown'
    if 'artist' in song:
        if type( song['artist'] ) is list:
            artist = ' - '.join( song['artist'] )
        else:
            artist = song['artist']
    if 'album' in song:
        if type( song['album'] ) is list:
            album = ' - '.join( song['album'] )
        else:
            album = song['album']
    artists.add( artist )
    # This is kind of a mistake since there could be multiple albums
    # with the same name. It doesn't appear that I have any, though.
    albums.add( album )
    catalog.add( ( artist, album ) )

database = sqlite3.connect( 'album_art.sqlite3' )
# Allows storage of UTF-8 strings in the database
database.text_factory = str
cursor   = database.cursor()

cursor.execute( "CREATE TABLE artists ( name text )" )
for artist in artists:
    cursor.execute( "INSERT INTO artists VALUES (?)", (artist, ) )

cursor.execute( "CREATE TABLE albums ( title text, cover_format text, cover_image blob )" )

for album in albums:
    cursor.execute( "INSERT INTO albums VALUES( ?, ?, ? )",
                    ( album, None, None ) )

cursor.execute( "CREATE TABLE contributions ( artist integer, album integer )" )
for contribution in catalog:
    cursor.execute( "INSERT INTO contributions SELECT artists.ROWID, albums.ROWID FROM artists, albums WHERE artists.name = ? AND albums.title = ?",
                    contribution )

database.commit()
database.close()
