#! /usr/bin/python

# Rescan the MPD data and update the album image database.

from mpd import MPDClient
import sqlite3

# Start by loading the database and ensuring that artists and albums
# are numbered properly.

database = sqlite3.connect( 'album_art.sqlite3' )
# Allows storage of UTF-8 strings in the database
database.text_factory = str
cursor   = database.cursor()

db_artists = dict()
db_albums  = dict()
db_catalog = set()

cursor.execute( "SELECT ROWID, name FROM artists" )

for artist in cursor:
    db_artists[artist[1]] = artist[0]

cursor.execute( "SELECT ROWID, title FROM albums" )

for album in cursor:
    db_albums[album[1]] = album[0]

cursor.execute( "SELECT artists.name, albums.title FROM artists, albums, contributions WHERE contributions.artist = artists.ROWID AND contributions.album = albums.ROWID" )

for contribution in cursor:
    db_catalog.add( contribution )

# Next get the current database from MPD.

player = MPDClient()
player.connect( 'guanaco', 6600 )

songs = player.listallinfo()

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
    if artist not in db_artists:
        print "Did not find artist: ", artist
        db_artists[artist] = 0
        cursor.execute( "INSERT INTO artists VALUES (?)", (artist, ) )
    if album not in db_albums:
        print "Did not find album: ", album
        db_albums[album] = 0
        cursor.execute( "INSERT INTO albums VALUES (?, ?, ? )",
                        ( album, None, None ) )
    if ( artist, album ) not in db_catalog:
        print "Did not find contribution: ", artist, album
        contribution = ( artist, album )
        db_catalog.add( contribution )
        cursor.execute( "INSERT INTO contributions SELECT artists.ROWID, albums.ROWID FROM artists, albums WHERE artists.name = ? AND albums.title = ?",
                        contribution )

database.commit()
database.close()
