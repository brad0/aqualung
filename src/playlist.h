/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/


#ifndef _PLAYLIST_H
#define _PLAYLIST_H


long get_playing_pos(GtkListStore * store);
void create_playlist(void);
void show_playlist(void);
void hide_playlist(void);
void set_playlist_color(void);
void save_playlist(char * filename);
void load_playlist(char * filename, int enqueue);
void add_to_playlist(char * filename, int enqueue);


#endif /* _PLAYLIST_H */