#!/bin/bash
# Simple script to add an image to the the database.
echo "adding $1 to album $2"
echo "UPDATE albums SET cover_format='JPG', cover_image=X'$(od -A n -t x1 -v $1 | tr -d '\r\n\t\ ')' WHERE albums.title LIKE '$2';" | sqlite3 album_art.sqlite3
