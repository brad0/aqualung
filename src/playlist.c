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


#include <config.h>

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <pthread.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>

#include "common.h"
#include "core.h"
#include "gui_main.h"
#include "music_browser.h"
#include "i18n.h"
#include "playlist.h"


extern char pl_color_active[14];
extern char pl_color_inactive[14];

extern char currdir[MAXLEN];
extern char cwd[MAXLEN];

int auto_save_playlist = 1;

GtkWidget * playlist_window;

int playlist_pos_x;
int playlist_pos_y;
int playlist_size_x;
int playlist_size_y;
int playlist_on;
int playlist_color_is_set;

extern int drift_x;
extern int drift_y;

extern int main_pos_x;
extern int main_pos_y;

extern int browser_pos_x;
extern int browser_pos_y;
extern int browser_on;

extern gulong play_id;
extern GtkWidget * play_button;

GtkWidget * play_list;
GtkListStore * play_store = 0;
GtkTreeSelection * play_select;

/* popup menus */
GtkWidget * sel_menu;
GtkWidget * sel__none;
GtkWidget * sel__all;
GtkWidget * sel__inv;
GtkWidget * rem_menu;
GtkWidget * rem__all;
GtkWidget * rem__sel;
GtkWidget * plist_menu;
GtkWidget * plist__save;
GtkWidget * plist__load;
GtkWidget * plist__enqueue;

char command[RB_CONTROL_SIZE];


extern int is_file_loaded;
extern int is_paused;
extern int allow_seeks;

extern pthread_mutex_t disk_thread_lock;
extern pthread_cond_t  disk_thread_wake;
extern jack_ringbuffer_t * rb_gui2disk;

extern GtkWidget * playlist_toggle;

extern int drag_info;


void rem__sel_cb(gpointer data);


void
set_playlist_color() {

	GtkTreeIter iter;
	char * str;
	char active[14];
	char inactive[14];
	int i = 0;

	sprintf(active, "#%04X%04X%04X",
		play_list->style->fg[SELECTED].red,
		play_list->style->fg[SELECTED].green,
		play_list->style->fg[SELECTED].blue);

	sprintf(inactive, "#%04X%04X%04X",
		play_list->style->fg[INSENSITIVE].red,
		play_list->style->fg[INSENSITIVE].green,
		play_list->style->fg[INSENSITIVE].blue);

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(play_store), &iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 2, &str, -1);

			if (strcmp(str, pl_color_active) == 0) {
				gtk_list_store_set(play_store, &iter, 2, active, -1);
				g_free(str);
			}

			if (strcmp(str, pl_color_inactive) == 0) {
				gtk_list_store_set(play_store, &iter, 2, inactive, -1);
				g_free(str);
			}

		} while (i++, gtk_tree_model_iter_next(GTK_TREE_MODEL(play_store), &iter));
	}

	strcpy(pl_color_active, active);
	strcpy(pl_color_inactive, inactive);
}


long
get_playing_pos(GtkListStore * store) {

	GtkTreeIter iter;
	char * str;
	int i = 0;

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 2, &str, -1);
			if (strcmp(str, pl_color_active) == 0) {
				g_free(str);
				return i;
			}

			g_free(str);
		
		} while (i++, gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
	}
	return -1;
}


void 
start_playback_from_playlist(GtkTreePath * path) {

	GtkTreeIter iter;
	char * str;
	long n;

	if ((is_paused) || (!allow_seeks))
		return;
	
	n = get_playing_pos(play_store);
	if (n != -1) {
		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n);
		gtk_list_store_set(play_store, &iter, 2, pl_color_inactive, -1);
	}
	
	gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, path);
	gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
	
	n = get_playing_pos(play_store);
	gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n);
	gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 1, &str, -1);
	
	g_signal_handler_block(G_OBJECT(play_button), play_id);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button), TRUE);
	g_signal_handler_unblock(G_OBJECT(play_button), play_id);
	
	command[0] = CMD_CUE;
	command[1] = '\0';
	strcat(command, str);
	g_free(str);
	
	jack_ringbuffer_write(rb_gui2disk, command, strlen(command));
	if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
		pthread_cond_signal(&disk_thread_wake);
		pthread_mutex_unlock(&disk_thread_lock);
	}
	
	is_file_loaded = 1;
}


static gboolean
playlist_window_close(GtkWidget * widget, GdkEvent * event, gpointer data) {

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_toggle), FALSE);

	return TRUE;
}


gint
playlist_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	GtkTreePath * path;
	GtkTreeViewColumn * column;
	GtkTreeIter iter;

	int i;

	switch (kevent->keyval) {
	case GDK_q:
	case GDK_Q:
		playlist_window_close(NULL, NULL, NULL);
		return TRUE;
		break;
	case GDK_Return:
	case GDK_KP_Enter:
		gtk_tree_view_get_cursor(GTK_TREE_VIEW(play_list), &path, &column);

		if (path) {
			start_playback_from_playlist(path);
		}		
		return TRUE;
		break;
	case GDK_Delete:
	case GDK_KP_Delete:
		i = 0;
		do {
			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++);
		} while (!gtk_tree_selection_iter_is_selected(play_select, &iter));
		
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(play_store), &iter);

		rem__sel_cb(NULL);

		gtk_tree_selection_select_path(play_select, path);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(play_list), path, NULL, FALSE);

		break;
	}

	return FALSE;
}


gint
doubleclick_handler(GtkWidget * widget, GdkEventButton * event, gpointer func_data) {

	GtkTreePath * path;
	GtkTreeViewColumn * column;

	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		
		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(play_list), event->x, event->y,
						  &path, &column, NULL, NULL)) {
			start_playback_from_playlist(path);
		}
	}

	if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		gtk_menu_popup(GTK_MENU(plist_menu), NULL, NULL, NULL, NULL,
			       event->button, event->time);
		return TRUE;
	}

	return FALSE;
}


void
plist__save_cb(gpointer data) {

        GtkWidget * file_selector;
        const gchar * selected_filename;
	char filename[MAXLEN];
        char * c;

        file_selector = gtk_file_selection_new(_("Please specify the file to save the playlist to."));
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector), currdir);
        gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(file_selector));
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector), "playlist.xml");
        gtk_widget_show(file_selector);

        if (gtk_dialog_run(GTK_DIALOG(file_selector)) == GTK_RESPONSE_OK) {
                selected_filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector));

		strncpy(filename, selected_filename, MAXLEN-1);
		save_playlist(filename);
                strncpy(currdir, gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector)),
                                                                 MAXLEN-1);
                if (currdir[strlen(currdir)-1] != '/') {
                        c = strrchr(currdir, '/');
                        if (*(++c))
                                *c = '\0';
                }
        }
        gtk_widget_destroy(file_selector);
}


void
plist__load_cb(gpointer data) {

        GtkWidget * file_selector;
        const gchar * selected_filename;
	char filename[MAXLEN];
        char * c;

        file_selector = gtk_file_selection_new(_("Please specify the file to load the playlist from."));
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector), currdir);
        gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(file_selector));
        gtk_widget_show(file_selector);

        if (gtk_dialog_run(GTK_DIALOG(file_selector)) == GTK_RESPONSE_OK) {
                selected_filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector));

		strncpy(filename, selected_filename, MAXLEN-1);
		load_playlist(filename, 0);
                strncpy(currdir, gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector)),
                                                                 MAXLEN-1);
                if (currdir[strlen(currdir)-1] != '/') {
                        c = strrchr(currdir, '/');
                        if (*(++c))
                                *c = '\0';
                }
        }
        gtk_widget_destroy(file_selector);
}


void
plist__enqueue_cb(gpointer data) {

        GtkWidget * file_selector;
        const gchar * selected_filename;
	char filename[MAXLEN];
        char * c;

        file_selector = gtk_file_selection_new("Please specify the file to load the playlist from.");
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector), currdir);
        gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(file_selector));
        gtk_widget_show(file_selector);

        if (gtk_dialog_run(GTK_DIALOG(file_selector)) == GTK_RESPONSE_OK) {
                selected_filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector));

		strncpy(filename, selected_filename, MAXLEN-1);
		load_playlist(filename, 1);
                strncpy(currdir, gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector)),
                                                                 MAXLEN-1);
                if (currdir[strlen(currdir)-1] != '/') {
                        c = strrchr(currdir, '/');
                        if (*(++c))
                                *c = '\0';
                }
        }
        gtk_widget_destroy(file_selector);
}


static gboolean
sel_cb(GtkWidget * widget, GdkEvent * event) {

        if (event->type == GDK_BUTTON_PRESS) {
                GdkEventButton * bevent = (GdkEventButton *) event;

                if (bevent->button == 3) {

			gtk_menu_popup(GTK_MENU(sel_menu), NULL, NULL, NULL, NULL,
				       bevent->button, bevent->time);

			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}


static gboolean
rem_cb(GtkWidget * widget, GdkEvent * event) {

        if (event->type == GDK_BUTTON_PRESS) {
                GdkEventButton * bevent = (GdkEventButton *) event;

                if (bevent->button == 3) {

			gtk_menu_popup(GTK_MENU(rem_menu), NULL, NULL, NULL, NULL,
				       bevent->button, bevent->time);

			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}


int
browse_direct_clicked(GtkWidget * widget, gpointer * data) {

        GtkWidget * file_selector;
        gchar ** selected_filenames;
        GtkListStore * model = (GtkListStore *)data;
        GtkTreeIter iter;
        int i;
	char * c;

        file_selector = gtk_file_selection_new(_("Please select the audio files for direct adding."));
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector), currdir);
        gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(file_selector));
        gtk_file_selection_set_select_multiple(GTK_FILE_SELECTION(file_selector), TRUE);
        gtk_widget_show(file_selector);

        if (gtk_dialog_run(GTK_DIALOG(file_selector)) == GTK_RESPONSE_OK) {
                selected_filenames = gtk_file_selection_get_selections(GTK_FILE_SELECTION(file_selector));
                for (i = 0; selected_filenames[i] != NULL; i++) {
                        if (selected_filenames[i][strlen(selected_filenames[i])-1] != '/') {
                                gtk_list_store_append(model, &iter);
                                gtk_list_store_set(model, &iter, 0, selected_filenames[i], -1);
                        }
                }
                g_strfreev(selected_filenames);

                strncpy(currdir, gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector)),
                                                                 MAXLEN-1);
                if (currdir[strlen(currdir)-1] != '/') {
                        c = strrchr(currdir, '/');
                        if (*(++c))
                                *c = '\0';
                }
        }
        gtk_widget_destroy(file_selector);

        return 0;
}


void
clicked_direct_list_header(GtkWidget * widget, gpointer * data) {

        GtkListStore * model = (GtkListStore *)data;

        gtk_list_store_clear(model);
}


void
direct_add(GtkWidget * widget, gpointer * data) {

        GtkWidget * dialog;
        GtkWidget * list_label;
        GtkWidget * viewport;
	GtkWidget * scrolled_win;
        GtkWidget * tracklist_tree;
        GtkListStore * model;
        GtkCellRenderer * cell;
        GtkTreeViewColumn * column;
        GtkTreeIter iter;
	GtkTreeIter play_iter;
        GtkWidget * browse_button;
        gchar * str;
	gchar * substr;
        int n, i;


        dialog = gtk_dialog_new_with_buttons(_("Direct add"),
                                             GTK_WINDOW(playlist_window),
                                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                             NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 300, 300);
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

        list_label = gtk_label_new(_("\nDirectly add these files to playlist:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), list_label, FALSE, TRUE, 2);

        viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 2);

	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_win);

        /* setup track list */
        model = gtk_list_store_new(1, G_TYPE_STRING);
        tracklist_tree = gtk_tree_view_new();
        gtk_tree_view_set_model(GTK_TREE_VIEW(tracklist_tree), GTK_TREE_MODEL(model));
        gtk_container_add(GTK_CONTAINER(scrolled_win), tracklist_tree);
        gtk_widget_set_size_request(tracklist_tree, 250, 50);

        cell = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_("Clear list"), cell, "text", 0, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tracklist_tree), GTK_TREE_VIEW_COLUMN(column));
        gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tracklist_tree), TRUE);
        g_signal_connect(G_OBJECT(column->button), "clicked", G_CALLBACK(clicked_direct_list_header),
                         (gpointer *)model);
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), 0, GTK_SORT_ASCENDING);

        browse_button = gtk_button_new_with_label(_("Add files..."));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), browse_button, FALSE, TRUE, 2);
        g_signal_connect(G_OBJECT(browse_button), "clicked", G_CALLBACK(browse_direct_clicked),
                         (gpointer *)model);

        gtk_widget_show_all(dialog);


        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                n = 0;
                if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter)) {
                        /* count the number of list items */
                        n = 1;
                        while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter))
                                n++;
                }
                if (n) {

                        gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter);
                        for (i = 0; i < n; i++) {
                                gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &str, -1);
                                gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);

				if ((substr = strrchr(str, '/')) == NULL)
					substr = str;
				else
					++substr;

				gtk_list_store_append(play_store, &play_iter);
				gtk_list_store_set(play_store, &play_iter, 0, substr, 1, str,
						   2, pl_color_inactive, -1);

                                g_free(str);
                        }
                }
        }

        gtk_widget_destroy(dialog);
}


void
select_all(GtkWidget * widget, gpointer * data) {

	gtk_tree_selection_select_all(play_select);
}


void
sel__all_cb(gpointer data) {

	select_all(NULL, NULL);
}


void
sel__none_cb(gpointer data) {

	gtk_tree_selection_unselect_all(play_select);
}


void
sel__inv_cb(gpointer data) {

	GtkTreeIter iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		if (gtk_tree_selection_iter_is_selected(play_select, &iter))
			gtk_tree_selection_unselect_iter(play_select, &iter);
		else
			gtk_tree_selection_select_iter(play_select, &iter);
			
	}
}


void
rem__all_cb(gpointer data) {

	gtk_list_store_clear(play_store);
}


void
rem__sel_cb(gpointer data) {

	GtkTreeIter iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {
			gtk_list_store_remove(play_store, &iter);
			--i;
		}
	}
}


void
remove_sel(GtkWidget * widget, gpointer * data) {

	rem__sel_cb(NULL);
}


gint
playlist_drag_data_received(GtkWidget * widget, GdkDragContext * drag_context, gint x, gint y, 
			    GtkSelectionData  * data, guint info, guint time) {

	GtkTreePath * path;
	GtkTreeViewColumn * column;
	GtkTreeIter iter;
	GtkTreeIter * piter = 0;

	if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(play_list), x, y, &path, &column, NULL, NULL)) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, path);
		piter = &iter;
	}

	switch (drag_info) {
	case 1:
		artist__addlist_cb(piter);
		break;
	case 2:
		record__addlist_cb(piter);
		break;
	case 3:
		track__addlist_cb(piter);
		break;
	}

	return FALSE;
}


void
create_playlist(void) {

	GtkWidget * vbox;
	GtkWidget * hbox_bottom;
	GtkWidget * direct_button;
	GtkWidget * selall_button;
	GtkWidget * remsel_button;

	GtkWidget * viewport;
	GtkWidget * scrolled_win;

        GtkCellRenderer * renderer;
        GtkTreeViewColumn * column;


        /* window creating stuff */
        playlist_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(playlist_window), _("Playlist"));
	gtk_window_set_gravity(GTK_WINDOW(playlist_window), GDK_GRAVITY_STATIC);
        g_signal_connect(G_OBJECT(playlist_window), "delete_event", G_CALLBACK(playlist_window_close), NULL);
        g_signal_connect(G_OBJECT(playlist_window), "key_press_event",
			 G_CALLBACK(playlist_window_key_pressed), NULL);
        gtk_container_set_border_width(GTK_CONTAINER(playlist_window), 2);
        gtk_widget_set_size_request(playlist_window, 300, 200);


	plist_menu = gtk_menu_new();

	plist__save = gtk_menu_item_new_with_label(_("Save playlist"));
	plist__load = gtk_menu_item_new_with_label(_("Load playlist"));
	plist__enqueue = gtk_menu_item_new_with_label(_("Enqueue playlist"));

	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__save);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__load);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__enqueue);

	g_signal_connect_swapped(G_OBJECT(plist__save), "activate", G_CALLBACK(plist__save_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(plist__load), "activate", G_CALLBACK(plist__load_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(plist__enqueue), "activate", G_CALLBACK(plist__enqueue_cb), NULL);

	gtk_widget_show(plist__save);
	gtk_widget_show(plist__load);
	gtk_widget_show(plist__enqueue);

        vbox = gtk_vbox_new(FALSE, 2);
        gtk_container_add(GTK_CONTAINER(playlist_window), vbox);

        /* create playlist */
	if (!play_store) {
		play_store = gtk_list_store_new(3,
						G_TYPE_STRING,  /* track name */
						G_TYPE_STRING,  /* physical filename */
						G_TYPE_STRING); /* color for selections */
	}

        play_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(play_store));
	gtk_widget_set_name(play_list, "play_list");
	playlist_color_is_set = 0;
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes("Tracks",
                                                          renderer,
                                                          "text", 0,
                                                          NULL);

	gtk_tree_view_column_add_attribute(column, renderer, "foreground", 2);

        gtk_tree_view_append_column(GTK_TREE_VIEW(play_list), column);
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(play_list), FALSE);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(play_list), TRUE);

	g_signal_connect(G_OBJECT(play_list), "button_press_event", G_CALLBACK(doubleclick_handler), NULL);
	g_signal_connect(G_OBJECT(play_list), "drag_data_received",
			 G_CALLBACK(playlist_drag_data_received), NULL);


	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	viewport = gtk_viewport_new(NULL, NULL);

        gtk_box_pack_start(GTK_BOX(vbox), viewport, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(viewport), scrolled_win);
	gtk_container_add(GTK_CONTAINER(scrolled_win), play_list);

        play_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(play_list));
        gtk_tree_selection_set_mode(play_select, GTK_SELECTION_MULTIPLE);


	/* bottom area of playlist window */
        hbox_bottom = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox_bottom, FALSE, TRUE, 0);

	direct_button = gtk_button_new_with_label(_("Direct add"));
        gtk_box_pack_start(GTK_BOX(hbox_bottom), direct_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(direct_button), "clicked", G_CALLBACK(direct_add), NULL);

	selall_button = gtk_button_new_with_label(_("Select all"));
        gtk_box_pack_start(GTK_BOX(hbox_bottom), selall_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(selall_button), "clicked", G_CALLBACK(select_all), NULL);
	
	remsel_button = gtk_button_new_with_label(_("Remove selected"));
        gtk_box_pack_start(GTK_BOX(hbox_bottom), remsel_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(remsel_button), "clicked", G_CALLBACK(remove_sel), NULL);
	
	

	/* create popup menus */
        sel_menu = gtk_menu_new();

        sel__none = gtk_menu_item_new_with_label(_("Select none"));
        gtk_menu_shell_append(GTK_MENU_SHELL(sel_menu), sel__none);
        g_signal_connect_swapped(G_OBJECT(sel__none), "activate", G_CALLBACK(sel__none_cb), NULL);
	gtk_widget_show(sel__none);

        sel__all = gtk_menu_item_new_with_label(_("Select all"));
        gtk_menu_shell_append(GTK_MENU_SHELL(sel_menu), sel__all);
        g_signal_connect_swapped(G_OBJECT(sel__all), "activate", G_CALLBACK(sel__all_cb), NULL);
	gtk_widget_show(sel__all);

        sel__inv = gtk_menu_item_new_with_label(_("Invert selection"));
        gtk_menu_shell_append(GTK_MENU_SHELL(sel_menu), sel__inv);
        g_signal_connect_swapped(G_OBJECT(sel__inv), "activate", G_CALLBACK(sel__inv_cb), NULL);
	gtk_widget_show(sel__inv);

        g_signal_connect_swapped(G_OBJECT(selall_button), "event", G_CALLBACK(sel_cb), NULL);


        rem_menu = gtk_menu_new();

        rem__all = gtk_menu_item_new_with_label(_("Remove all"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), rem__all);
        g_signal_connect_swapped(G_OBJECT(rem__all), "activate", G_CALLBACK(rem__all_cb), NULL);
	gtk_widget_show(rem__all);

        rem__sel = gtk_menu_item_new_with_label(_("Remove selected"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), rem__sel);
        g_signal_connect_swapped(G_OBJECT(rem__sel), "activate", G_CALLBACK(rem__sel_cb), NULL);
	gtk_widget_show(rem__sel);

        g_signal_connect_swapped(G_OBJECT(remsel_button), "event", G_CALLBACK(rem_cb), NULL);
}


void
show_playlist(void) {

	playlist_on = 1;

        gtk_widget_show_all(playlist_window);
	gtk_window_move(GTK_WINDOW(playlist_window), playlist_pos_x, playlist_pos_y);
	gtk_window_resize(GTK_WINDOW(playlist_window), playlist_size_x, playlist_size_y);

	if (!playlist_color_is_set) {
		set_playlist_color();
		playlist_color_is_set = 1;
	}
}


void
hide_playlist(void) {

	playlist_on = 0;
	gtk_window_get_position(GTK_WINDOW(playlist_window), &playlist_pos_x, &playlist_pos_y);
	gtk_window_get_size(GTK_WINDOW(playlist_window), &playlist_size_x, &playlist_size_y);
        gtk_widget_hide(playlist_window);
}


void
save_playlist(char * filename) {

        int i = 0;
        GtkTreeIter iter;
	char * track_name;
	char * phys_name;
	char * color;
        xmlDocPtr doc;
        xmlNodePtr root;
        xmlNodePtr node;
        char str[32];


        doc = xmlNewDoc("1.0");
        root = xmlNewNode(NULL, "aqualung_playlist");
        xmlDocSetRootElement(doc, root);

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i)) {

                gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter,
				   0, &track_name, 1, &phys_name, 2, &color, -1);

                node = xmlNewTextChild(root, NULL, "track", NULL);

                xmlNewTextChild(node, NULL, "track_name", track_name);
                xmlNewTextChild(node, NULL, "phys_name", phys_name);

		if (strcmp(color, pl_color_active) == 0) {
			strcpy(str, "yes");
		} else {
			strcpy(str, "no");
		}

                xmlNewTextChild(node, NULL, "is_active", str);

		g_free(track_name);
		g_free(phys_name);
		g_free(color);
                ++i;
        }
        xmlSaveFormatFile(filename, doc, 1);
}


void
parse_playlist_track(xmlDocPtr doc, xmlNodePtr cur, int sel_ok) {

        xmlChar * key;
	GtkTreeIter iter;
	char track_name[MAXLEN];
	char phys_name[MAXLEN];
	char color[32];

	track_name[0] = '\0';
	phys_name[0] = '\0';
	color[0] = '\0';
	
        cur = cur->xmlChildrenNode;
        while (cur != NULL) {
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"track_name"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(track_name, key, MAXLEN-1);
                        xmlFree(key);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"phys_name"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(phys_name, key, MAXLEN-1);
                        xmlFree(key);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"is_active"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				if ((xmlStrcmp(key, (const xmlChar *)"yes")) || (!sel_ok)) {
					strncpy(color, pl_color_inactive, 31);
				} else {
					strncpy(color, pl_color_active, 31);
				}
			}
                        xmlFree(key);
                }
		cur = cur->next;
	}

	if ((track_name[0] != '\0') && (phys_name[0] != '\0') && (color[0] != '\0')) {
		gtk_list_store_append(play_store, &iter);
		gtk_list_store_set(play_store, &iter, 0, track_name, 1, phys_name, 2, color, -1);
	} else {
		fprintf(stderr, "warning: parse_playlist_track: incomplete data in playlist XML\n");
	}
}


void
load_playlist(char * filename, int enqueue) {

        xmlDocPtr doc;
        xmlNodePtr cur;
	FILE * f;
	int sel_ok = 0;

        if ((f = fopen(filename, "rt")) == NULL) {
                return;
        }
        fclose(f);

        doc = xmlParseFile(filename);
        if (doc == NULL) {
                fprintf(stderr, "An XML error occured while parsing %s\n", filename);
                return;
        }

        cur = xmlDocGetRootElement(doc);
        if (cur == NULL) {
                fprintf(stderr, "load_playlist: empty XML document\n");
                xmlFreeDoc(doc);
                return;
        }

        if (xmlStrcmp(cur->name, (const xmlChar *)"aqualung_playlist")) {
                fprintf(stderr, "load_playlist: XML document of the wrong type, "
                        "root node != aqualung_playlist\n");
                xmlFreeDoc(doc);
                return;
        }

	if (pl_color_active[0] == '\0')
		strcpy(pl_color_active, "#ffffff");
	if (pl_color_inactive[0] == '\0')
		strcpy(pl_color_inactive, "#000000");

	if (!enqueue)
		gtk_list_store_clear(play_store);

	if (get_playing_pos(play_store) == -1)
		sel_ok = 1;
		
        cur = cur->xmlChildrenNode;
        while (cur != NULL) {
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"track"))) {
                        parse_playlist_track(doc, cur, sel_ok);
                }
                cur = cur->next;
        }

        xmlFreeDoc(doc);
}


int
is_playlist(char * filename) {

	FILE * f;
	char buf[] = "<?xml version=\"1.0\"?>\n<aqualung_playlist>\0";
	char inbuf[64];

	if ((f = fopen(filename, "rb")) == NULL) {
		return 0;
	}
	if (fread(inbuf, 1, strlen(buf)+1, f) != strlen(buf)+1) {
		fclose(f);
		return 0;
	}
	fclose(f);
	inbuf[strlen(buf)] = '\0';

	if (strcmp(buf, inbuf) == 0) {
		return 1;
	}

	return 0;
}


void
add_to_playlist(char * filename, int enqueue) {

	char fullname[MAXLEN];
	char * endname;
	char * home;
	char * path = filename;
	GtkTreeIter iter;


	if (!filename)
		return;


	switch (filename[0]) {
	case '/':
		strcpy(fullname, filename);
		break;
	case '~':
		++path;
		if (!(home = getenv("HOME"))) {
			fprintf(stderr, "add_to_playlist(): cannot resolve home directory\n");
			return;
		}
		snprintf(fullname, MAXLEN-1, "%s/%s", home, path);
		break;
	default:
		snprintf(fullname, MAXLEN-1, "%s/%s", cwd, filename);
		break;
	}


        if (pl_color_active[0] == '\0')
                strcpy(pl_color_active, "#ffffff");
        if (pl_color_inactive[0] == '\0')
                strcpy(pl_color_inactive, "#000000");

	if (is_playlist(fullname)) {
		load_playlist(fullname, enqueue);
	} else {
		if (!enqueue)
			gtk_list_store_clear(play_store);

		if ((endname = strrchr(fullname, '/')) == NULL) {
			endname = fullname;
		} else {
			++endname;
		}
                gtk_list_store_append(play_store, &iter);
                gtk_list_store_set(play_store, &iter, 0, endname, 1, fullname, 2, pl_color_inactive, -1);
	}
}