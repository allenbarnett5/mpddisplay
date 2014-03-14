#!/bin/bash
# Simple script to add an image to the the database.
echo "adding $1 to album $2"
# About all this can do is say whether or not something is found.
# A failure to find a matching file doesn't return 1 or any other
# process exit value (just 0).
sqlite3 album_art.sqlite3 "SELECT * FROM albums WHERE albums.title LIKE '$2';"
# The "echo" avoids generating a really long command line.
echo "UPDATE albums SET cover_format='JPG', cover_image=X'$(od -A n -t x1 -v $1 | tr -d '\r\n\t\ ')' WHERE albums.title LIKE '$2';" | sqlite3 album_art.sqlite3
