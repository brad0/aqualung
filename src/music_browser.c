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
#include <stdlib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "common.h"
#include "drag.xpm"
#include "gui_main.h"
#include "i18n.h"
#include "music_browser.h"


extern char pl_color_active[14];
extern char pl_color_inactive[14];

extern char confdir[MAXLEN];
extern char currdir[MAXLEN];

char title_format[MAXLEN];

GtkWidget * browser_window;

int browser_pos_x;
int browser_pos_y;
int browser_size_x;
int browser_size_y;
int browser_on;

extern int drift_x;
extern int drift_y;

extern int main_pos_x;
extern int main_pos_y;

extern int playlist_pos_x;
extern int playlist_pos_y;
extern int playlist_on;

extern GtkWidget * play_list;

GtkWidget * music_tree;
GtkTreeStore * music_store = 0;
GtkTreeSelection * music_select;

GtkWidget * comment_view;

extern GtkListStore * play_store;

extern GtkWidget * musicstore_toggle;

/* popup menus for tree items */
GtkWidget * artist_menu;
GtkWidget * artist__addlist;
GtkWidget * artist__separator1;
GtkWidget * artist__add;
GtkWidget * artist__edit;
GtkWidget * artist__addrec;
GtkWidget * artist__separator2;
GtkWidget * artist__remove;

GtkWidget * record_menu;
GtkWidget * record__addlist;
GtkWidget * record__separator1;
GtkWidget * record__add;
GtkWidget * record__edit;
GtkWidget * record__addtrk;
GtkWidget * record__separator2;
GtkWidget * record__remove;

GtkWidget * track_menu;
GtkWidget * track__addlist;
GtkWidget * track__separator1;
GtkWidget * track__add;
GtkWidget * track__edit;
GtkWidget * track__separator2;
GtkWidget * track__remove;

GtkWidget * blank_menu;
GtkWidget * blank__add;


/* prototypes, when we need them */
void load_music_store(void);
static void tree_selection_changed_cb(GtkTreeSelection * selection, gpointer data);

static gboolean music_tree_event_cb(GtkWidget * widget, GdkEvent * event);

static void artist__add_cb(gpointer data);
static void artist__edit_cb(gpointer data);
static void artist__remove_cb(gpointer data);

static void record__add_cb(gpointer data);
static void record__edit_cb(gpointer data);
static void record__remove_cb(gpointer data);

static void track__add_cb(gpointer data);
static void track__edit_cb(gpointer data);
static void track__remove_cb(gpointer data);

gint playlist_drag_data_received(GtkWidget * widget, GdkDragContext * drag_context, gint x,
				 gint y, GtkSelectionData  * data, guint info, guint time);


struct keybinds {
	void (*callback)(gpointer);
	int keyval1;
	int keyval2;
};

struct keybinds artist_keybinds[] = {
	{artist__addlist_cb, GDK_a, GDK_A},
	{artist__add_cb, GDK_n, GDK_N},
	{artist__edit_cb, GDK_e, GDK_E},
	{artist__remove_cb, GDK_Delete, GDK_KP_Delete},
	{record__add_cb, GDK_plus, GDK_KP_Add},
	{NULL, 0}
};

struct keybinds record_keybinds[] = {
	{record__addlist_cb, GDK_a, GDK_A},
	{record__add_cb, GDK_n, GDK_N},
	{record__edit_cb, GDK_e, GDK_E},
	{record__remove_cb, GDK_Delete, GDK_KP_Delete},
	{track__add_cb, GDK_plus, GDK_KP_Add},
	{NULL, 0}
};

struct keybinds track_keybinds[] = {
	{track__addlist_cb, GDK_a, GDK_A},
	{track__add_cb, GDK_n, GDK_N},
	{track__edit_cb, GDK_e, GDK_E},
	{track__remove_cb, GDK_Delete, GDK_KP_Delete},
	{NULL, 0}
};


GtkTargetEntry target_table[] = {
	{ "", GTK_TARGET_SAME_APP, 0 }
};

int drag_info;


static gboolean
browser_window_close(GtkWidget * widget, GdkEvent * event, gpointer data) {

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(musicstore_toggle), FALSE);

	return TRUE;
}

gint
browser_window_key_pressed(GtkWidget * widget, GdkEventKey * event) {

	switch (event->keyval) {
	case GDK_q:
	case GDK_Q:
		browser_window_close(NULL, NULL, NULL);
		return TRUE;
		break;
	}

	return FALSE;
}


void
make_title_string(char * dest, char * template, char * artist, char * record, char * track) {

	int i;
	int j;
	int argc = 0;
	char * arg[3] = { "", "", "" };
	char temp[MAXLEN];

	temp[0] = template[0];

	for (i = j = 1; i < strlen(template) && j < MAXLEN - 1; i++, j++) {
		if (template[i - 1] == '%') {
			if (argc < 3) {
				switch (template[i]) {
				case 'a':
					arg[argc++] = artist;
					temp[j] = 's';
					break;
				case 'r':
					arg[argc++] = record;
					temp[j] = 's';
					break;
				case 't':
					arg[argc++] = track;
					temp[j] = 's';
					break;
				default:
					temp[j++] = '%';
					temp[j] = template[i];
					break;
				}
			} else {
				temp[j++] = '%';
				temp[j] = template[i];
			}
		} else {
			temp[j] = template[i];
		}
	}

	temp[j] = '\0';
	
	snprintf(dest, MAXLEN - 1, temp, arg[0], arg[1], arg[2]);
}


int
add_artist_dialog(char * name, char * sort_name, char * comment) {

        GtkWidget * dialog;
        GtkWidget * name_entry;
	GtkWidget * name_label;
	GtkWidget * sort_name_entry;
	GtkWidget * sort_name_label;
	GtkWidget * viewport;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

        int ret;

        dialog = gtk_dialog_new_with_buttons(_("Add Artist"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 300, 300);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	name_label = gtk_label_new(_("\nVisible name:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), name_label, FALSE, TRUE, 2);

        name_entry = gtk_entry_new_with_max_length(MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(name_entry), name);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), name_entry, FALSE, TRUE, 2);

	sort_name_label = gtk_label_new(_("\nName to sort by:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), sort_name_label, FALSE, TRUE, 2);

        sort_name_entry = gtk_entry_new_with_max_length(MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(sort_name_entry), sort_name);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), sort_name_entry, FALSE, TRUE, 2);

	comment_label = gtk_label_new(_("\nComments:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), comment_label, FALSE, TRUE, 2);


	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 2);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_window);

        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), comment, -1);
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(dialog);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
                strcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)));
                strcpy(sort_name, gtk_entry_get_text(GTK_ENTRY(sort_name_entry)));

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));

		if (name[0] != '\0')
			ret = 1;
		else
			ret = 0;
        } else {
                name[0] = '\0';
                sort_name[0] = '\0';
                comment[0] = '\0';
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


int
edit_artist_dialog(char * name, char * sort_name, char * comment) {

        GtkWidget * dialog;
        GtkWidget * name_entry;
	GtkWidget * name_label;
	GtkWidget * sort_name_entry;
	GtkWidget * sort_name_label;
	GtkWidget * viewport;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

        int ret;

        dialog = gtk_dialog_new_with_buttons(_("Edit Artist"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 300, 300);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	name_label = gtk_label_new(_("\nVisible name:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), name_label, FALSE, TRUE, 2);

        name_entry = gtk_entry_new_with_max_length(MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(name_entry), name);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), name_entry, FALSE, TRUE, 2);

	sort_name_label = gtk_label_new(_("\nName to sort by:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), sort_name_label, FALSE, TRUE, 2);

        sort_name_entry = gtk_entry_new_with_max_length(MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(sort_name_entry), sort_name);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), sort_name_entry, FALSE, TRUE, 2);

	comment_label = gtk_label_new(_("\nComments:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), comment_label, FALSE, TRUE, 2);

	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 2);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_window);


        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), comment, -1);
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(dialog);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
                strcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)));
                strcpy(sort_name, gtk_entry_get_text(GTK_ENTRY(sort_name_entry)));

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));
		if (name[0] != '\0')
			ret = 1;
		else
			ret = 0;
        } else {
                name[0] = '\0';
                sort_name[0] = '\0';
                comment[0] = '\0';
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


int
browse_button_record_clicked(GtkWidget * widget, gpointer * data) {

	GtkWidget * file_selector;
	gchar ** selected_filenames;
	GtkListStore * model = (GtkListStore *)data;
	GtkTreeIter iter;
	int i;
	char * c;

	file_selector = gtk_file_selection_new(_("Please select the audio files for this record."));
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
clicked_tracklist_header(GtkWidget * widget, gpointer * data) {

	GtkListStore * model = (GtkListStore *)data;

	gtk_list_store_clear(model);
}


int
add_record_dialog(char * name, char * sort_name, char *** strings, char * comment) {

        GtkWidget * dialog;
        GtkWidget * name_entry;
	GtkWidget * name_label;
	GtkWidget * sort_name_entry;
	GtkWidget * sort_name_label;
	GtkWidget * list_label;

	GtkWidget * viewport1;
	GtkWidget * scrolled_win;
	GtkWidget * tracklist_tree;
	GtkListStore * model;
	GtkCellRenderer * cell;
	GtkTreeViewColumn * column;
	GtkTreeIter iter;
	gchar * str;

	GtkWidget * browse_button;
	GtkWidget * viewport2;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;
	int n, i;

        int ret;

        dialog = gtk_dialog_new_with_buttons(_("Add Record"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 400, 400);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	name_label = gtk_label_new(_("\nVisible name:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), name_label, FALSE, TRUE, 2);

        name_entry = gtk_entry_new_with_max_length(MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(name_entry), name);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), name_entry, FALSE, TRUE, 2);

	sort_name_label = gtk_label_new(_("\nName to sort by:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), sort_name_label, FALSE, TRUE, 2);

        sort_name_entry = gtk_entry_new_with_max_length(MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(sort_name_entry), sort_name);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), sort_name_entry, FALSE, TRUE, 2);


	list_label = gtk_label_new(_("\nAuto-create tracks from these files:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), list_label, FALSE, TRUE, 2);

	viewport1 = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport1, TRUE, TRUE, 2);

	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport1), scrolled_win);

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
        g_signal_connect(G_OBJECT(column->button), "clicked", G_CALLBACK(clicked_tracklist_header),
			 (gpointer *)model);
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), 0, GTK_SORT_ASCENDING);

	browse_button = gtk_button_new_with_label(_("Add files..."));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), browse_button, FALSE, TRUE, 2);
        g_signal_connect(G_OBJECT(browse_button), "clicked", G_CALLBACK(browse_button_record_clicked),
			 (gpointer *)model);

	comment_label = gtk_label_new(_("\nComments:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), comment_label, FALSE, TRUE, 2);


	viewport2 = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport2, TRUE, TRUE, 2);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport2), scrolled_window);

        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), comment, -1);
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(dialog);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
                strcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)));
                strcpy(sort_name, gtk_entry_get_text(GTK_ENTRY(sort_name_entry)));

		n = 0;
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter)) {
			/* count the number of list items */
			n = 1;
			while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter))
				n++;
		}
		if ((n) && (name[0] != '\0')) {

			gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter);
			if (!(*strings = calloc(n + 1, sizeof(char *)))) {
				fprintf(stderr, "add_record_dialog(): calloc error\n");
				exit(1);
			}
			for (i = 0; i < n; i++) {
				gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &str, -1);
				gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);

				if (!((*strings)[i] = calloc(strlen(str)+1, sizeof(char)))) {
					fprintf(stderr, "add_record_dialog(): calloc error\n");
					exit(1);
				}

				strcpy((*strings)[i], str);
				g_free(str);
			}
			(*strings)[i] = NULL;
		}

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));
		if (name[0] != '\0')
			ret = 1;
		else
			ret = 0;
        } else {
                name[0] = '\0';
                sort_name[0] = '\0';
                comment[0] = '\0';
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


int
edit_record_dialog(char * name, char * sort_name, char * comment) {

        GtkWidget * dialog;
        GtkWidget * name_entry;
	GtkWidget * name_label;
	GtkWidget * sort_name_entry;
	GtkWidget * sort_name_label;
	GtkWidget * viewport;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

        int ret;

        dialog = gtk_dialog_new_with_buttons(_("Edit Record"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 300, 300);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	name_label = gtk_label_new(_("\nVisible name:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), name_label, FALSE, TRUE, 2);

        name_entry = gtk_entry_new_with_max_length(MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(name_entry), name);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), name_entry, FALSE, TRUE, 2);

	sort_name_label = gtk_label_new(_("\nName to sort by:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), sort_name_label, FALSE, TRUE, 2);

        sort_name_entry = gtk_entry_new_with_max_length(MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(sort_name_entry), sort_name);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), sort_name_entry, FALSE, TRUE, 2);

	comment_label = gtk_label_new(_("\nComments:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), comment_label, FALSE, TRUE, 2);


	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 2);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_window);

        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), comment, -1);
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(dialog);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
                strcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)));
                strcpy(sort_name, gtk_entry_get_text(GTK_ENTRY(sort_name_entry)));

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));
		if (name[0] != '\0')
			ret = 1;
		else
			ret = 0;
        } else {
                name[0] = '\0';
                sort_name[0] = '\0';
                comment[0] = '\0';
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


int
browse_button_track_clicked(GtkWidget * widget, gpointer * data) {

	GtkWidget * file_selector;
	const gchar * selected_filename = gtk_entry_get_text(GTK_ENTRY(data));
	char * c;

	file_selector = gtk_file_selection_new(_("Please select the audio file for this track."));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector), currdir);
	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(file_selector));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector), selected_filename);
	gtk_widget_show(file_selector);

        if (gtk_dialog_run(GTK_DIALOG(file_selector)) == GTK_RESPONSE_OK) {
		selected_filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector));
		gtk_entry_set_text(GTK_ENTRY(data), selected_filename);

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


int
add_track_dialog(char * name, char * sort_name, char * file, char * comment) {

        GtkWidget * dialog;
        GtkWidget * name_entry;
	GtkWidget * name_label;
	GtkWidget * sort_name_entry;
	GtkWidget * sort_name_label;
	GtkWidget * hbox;
	GtkWidget * file_entry;
	GtkWidget * file_label;
	GtkWidget * browse_button;
	GtkWidget * viewport;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

        int ret;

        dialog = gtk_dialog_new_with_buttons(_("Add Track"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 400, 350);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	name_label = gtk_label_new(_("\nVisible name:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), name_label, FALSE, TRUE, 2);

        name_entry = gtk_entry_new_with_max_length(MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(name_entry), name);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), name_entry, FALSE, TRUE, 2);

	sort_name_label = gtk_label_new(_("\nName to sort by:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), sort_name_label, FALSE, TRUE, 2);

        sort_name_entry = gtk_entry_new_with_max_length(MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(sort_name_entry), sort_name);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), sort_name_entry, FALSE, TRUE, 2);

	file_label = gtk_label_new(_("\nFilename:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), file_label, FALSE, TRUE, 2);


	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, TRUE, 2);

        file_entry = gtk_entry_new_with_max_length(MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(file_entry), file);
        gtk_box_pack_start(GTK_BOX(hbox), file_entry, TRUE, TRUE, 2);


	browse_button = gtk_button_new_with_label(_("Browse..."));
        gtk_box_pack_start(GTK_BOX(hbox), browse_button, FALSE, TRUE, 2);
        g_signal_connect(G_OBJECT(browse_button), "clicked", G_CALLBACK(browse_button_track_clicked),
			 (gpointer *)file_entry);
	

	comment_label = gtk_label_new(_("\nComments:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), comment_label, FALSE, TRUE, 2);

	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 2);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_window);

        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), comment, -1);
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(dialog);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                strcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)));
                strcpy(sort_name, gtk_entry_get_text(GTK_ENTRY(sort_name_entry)));
                strcpy(file, gtk_entry_get_text(GTK_ENTRY(file_entry)));

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));

		if ((name[0] != '\0') && (file[0] != '\0'))
			ret = 1;
		else
			ret = 0;
        } else {
                name[0] = '\0';
                sort_name[0] = '\0';
                file[0] = '\0';
                comment[0] = '\0';
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


int
edit_track_dialog(char * name, char * sort_name, char * file, char * comment) {

        GtkWidget * dialog;
        GtkWidget * name_entry;
	GtkWidget * name_label;
	GtkWidget * sort_name_entry;
	GtkWidget * sort_name_label;
	GtkWidget * hbox;
	GtkWidget * file_entry;
	GtkWidget * file_label;
	GtkWidget * browse_button;
	GtkWidget * viewport;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

        int ret;

        dialog = gtk_dialog_new_with_buttons(_("Edit Track"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 400, 350);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	name_label = gtk_label_new(_("\nVisible name:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), name_label, FALSE, TRUE, 2);

        name_entry = gtk_entry_new_with_max_length(MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(name_entry), name);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), name_entry, FALSE, TRUE, 2);

	sort_name_label = gtk_label_new(_("\nName to sort by:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), sort_name_label, FALSE, TRUE, 2);

        sort_name_entry = gtk_entry_new_with_max_length(MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(sort_name_entry), sort_name);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), sort_name_entry, FALSE, TRUE, 2);

	file_label = gtk_label_new(_("\nFilename:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), file_label, FALSE, TRUE, 2);


	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, TRUE, 2);

        file_entry = gtk_entry_new_with_max_length(MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(file_entry), file);
        gtk_box_pack_start(GTK_BOX(hbox), file_entry, TRUE, TRUE, 2);


	browse_button = gtk_button_new_with_label(_("Browse..."));
        gtk_box_pack_start(GTK_BOX(hbox), browse_button, FALSE, TRUE, 2);
        g_signal_connect(G_OBJECT(browse_button), "clicked", G_CALLBACK(browse_button_track_clicked),
			 (gpointer *)file_entry);
	

	comment_label = gtk_label_new(_("\nComments:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), comment_label, FALSE, TRUE, 2);

	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 2);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_window);

        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), comment, -1);
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(dialog);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                strcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)));
                strcpy(sort_name, gtk_entry_get_text(GTK_ENTRY(sort_name_entry)));
                strcpy(file, gtk_entry_get_text(GTK_ENTRY(file_entry)));

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));

		if ((name[0] != '\0') && (file[0] != '\0'))
			ret = 1;
		else
			ret = 0;
        } else {
                name[0] = '\0';
                sort_name[0] = '\0';
                file[0] = '\0';
                comment[0] = '\0';
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


int
confirm_dialog(char * title, char * text) {

        GtkWidget * dialog;
        GtkWidget * label;
        int ret;

        dialog = gtk_dialog_new_with_buttons(title,
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	label = gtk_label_new(text);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, TRUE, 10);
	gtk_widget_show(label);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
                ret = 1;
        } else {
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


static gboolean
music_tree_event_cb(GtkWidget * widget, GdkEvent * event) {

	GtkTreePath * path;
	GtkTreeViewColumn * column;

	if (event->type == GDK_BUTTON_PRESS) {
		GdkEventButton * bevent = (GdkEventButton *) event;

		if (bevent->button == 3) {

			if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(music_tree), bevent->x, bevent->y,
							  &path, &column, NULL, NULL)) {
				
				gtk_tree_view_set_cursor(GTK_TREE_VIEW(music_tree), path, NULL, FALSE);

				switch (gtk_tree_path_get_depth(path)) {
				case 1:
					gtk_menu_popup(GTK_MENU(artist_menu), NULL, NULL, NULL, NULL,
						       bevent->button, bevent->time);
					break;
				case 2:
					gtk_menu_popup(GTK_MENU(record_menu), NULL, NULL, NULL, NULL,
						       bevent->button, bevent->time);
					break;
				case 3:
					gtk_menu_popup(GTK_MENU(track_menu), NULL, NULL, NULL, NULL,
						       bevent->button, bevent->time);
					break;
				}
			} else {
				gtk_menu_popup(GTK_MENU(blank_menu), NULL, NULL, NULL, NULL,
					       bevent->button, bevent->time);
			}
			return TRUE;
		}
		return FALSE;
	} 

	if (event->type == GDK_KEY_PRESS) {
		GdkEventKey * kevent = (GdkEventKey *) event;

		GtkTreeIter iter;
		GtkTreeModel * model;

		if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {
			GtkTreePath * path = gtk_tree_model_get_path(model, &iter);
			int i;
			
			switch (kevent->keyval) {
			case GDK_KP_Enter:
			case GDK_Return:
				if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(music_tree), path)) {
					gtk_tree_view_collapse_row(GTK_TREE_VIEW(music_tree), path);
				} else {
					gtk_tree_view_expand_row(GTK_TREE_VIEW(music_tree), path, FALSE);
				}
				break;
			}

			switch (gtk_tree_path_get_depth(path)) {
			case 1:
				for (i = 0; artist_keybinds[i].callback; ++i)
					if (kevent->keyval == artist_keybinds[i].keyval1 ||
					    kevent->keyval == artist_keybinds[i].keyval2)
						(artist_keybinds[i].callback)(NULL);
				break;
			case 2:
				for (i = 0; record_keybinds[i].callback; ++i)
					if (kevent->keyval == record_keybinds[i].keyval1 ||
					    kevent->keyval == record_keybinds[i].keyval2)
						(record_keybinds[i].callback)(NULL);
				break;
			case 3:
				for (i = 0; track_keybinds[i].callback; ++i)
					if (kevent->keyval == track_keybinds[i].keyval1 ||
					    kevent->keyval == track_keybinds[i].keyval2)
						(track_keybinds[i].callback)(NULL);
				break;
			}
		}
		return FALSE;
	}
	return FALSE;
}


gint
dblclick_handler(GtkWidget * widget, GdkEventButton * event, gpointer func_data) {

        GtkTreePath * path;
        GtkTreeViewColumn * column;

        if ((event->type==GDK_2BUTTON_PRESS) && (event->button == 1)) {

		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(music_tree), event->x, event->y,
						  &path, &column, NULL, NULL)) {
			
			switch (gtk_tree_path_get_depth(path)) {
			case 1:
				artist__addlist_cb(NULL);
				return TRUE;
				break;
			case 2:
				record__addlist_cb(NULL);
				return TRUE;
				break;
			case 3:
				track__addlist_cb(NULL);
				return TRUE;
				break;
			default:
				return FALSE;
			}
		}
		return FALSE;
	}
	return FALSE;
}


/****************************************/


void
artist__addlist_cb(gpointer data) {

        GtkTreeIter iter_artist;
        GtkTreeIter iter_record;
        GtkTreeIter iter_track;
	GtkTreeIter list_iter;
        GtkTreeModel * model;

        char * partist_name;
        char * precord_name;
        char * ptrack_name;
        char * pfile;

        char artist_name[MAXLEN];
        char record_name[MAXLEN];
        char track_name[MAXLEN];
        char file[MAXLEN];

	char list_str[MAXLEN];
	int i, j;

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_artist)) {

                gtk_tree_model_get(model, &iter_artist, 0, &partist_name, -1);
                strncpy(artist_name, partist_name, MAXLEN-1);
                g_free(partist_name);

		i = 0;
		while (gtk_tree_model_iter_nth_child(model, &iter_record, &iter_artist, i++)) {

			gtk_tree_model_get(model, &iter_record, 0, &precord_name, -1);
			strncpy(record_name, precord_name, MAXLEN-1);
			g_free(precord_name);
			
			j = 0;
			while (gtk_tree_model_iter_nth_child(model, &iter_track, &iter_record, j++)) {
				
				gtk_tree_model_get(model, &iter_track, 0, &ptrack_name, 2, &pfile, -1);
				strncpy(track_name, ptrack_name, MAXLEN-1);
				strncpy(file, pfile, MAXLEN-1);
				g_free(ptrack_name);
				g_free(pfile);

				make_title_string(list_str, title_format,
						  artist_name, record_name, track_name);

				gtk_list_store_insert_before(play_store,
							     &list_iter,
							     (GtkTreeIter *)data);

				gtk_list_store_set(play_store, &list_iter, 0, list_str, 1, file,
						   2, pl_color_inactive, -1);
			}
		}
	}
}


static void
artist__add_cb(gpointer data) {

	GtkTreeIter iter;
	char name[MAXLEN];
	char sort_name[MAXLEN];
	char comment[MAXLEN];

	name[0] = '\0';
	sort_name[0] = '\0';
	comment[0] = '\0';

	if (add_artist_dialog(name, sort_name, comment)) {

		gtk_tree_store_append(music_store, &iter, NULL);
		gtk_tree_store_set(music_store, &iter, 0, name, 1, sort_name, 2, "", 3, comment, -1);
	}
}


static void
artist__edit_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	char * pname;
	char * psort_name;
	char * pcomment;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char comment[MAXLEN];
	
	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

                gtk_tree_model_get(model, &iter, 0, &pname, 1, &psort_name, 3, &pcomment, -1);

		strncpy(name, pname, MAXLEN-1);
		strncpy(sort_name, psort_name, MAXLEN-1);
		strncpy(comment, pcomment, MAXLEN-1);

                g_free(pname);
                g_free(psort_name);
                g_free(pcomment);

		if (edit_artist_dialog(name, sort_name, comment)) {

			gtk_tree_store_set(music_store, &iter, 0, name, 1, sort_name, 2, "", 3, comment, -1);
			tree_selection_changed_cb(music_select, NULL);
		}
        }
}


static void
artist__remove_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;
	char * pname;
	char name[MAXLEN];
	char text[MAXLEN];

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

                gtk_tree_model_get(model, &iter, 0, &pname, -1);
		strncpy(name, pname, MAXLEN-1);
                g_free(pname);
		
		snprintf(text, MAXLEN-1, _("Really remove \"%s\" from the Music Store?"), name);
		if (confirm_dialog(_("Remove Artist"), text)) {
			gtk_tree_store_remove(music_store, &iter);
			tree_selection_changed_cb(music_select, NULL);
		}
	}
}

/************************************/


void
record__addlist_cb(gpointer data) {

        GtkTreeIter iter_artist;
        GtkTreeIter iter_record;
        GtkTreeIter iter_track;
	GtkTreeIter list_iter;
        GtkTreeModel * model;

        char * partist_name;
        char * precord_name;
        char * ptrack_name;
        char * pfile;

        char artist_name[MAXLEN];
        char record_name[MAXLEN];
        char track_name[MAXLEN];
        char file[MAXLEN];

	char list_str[MAXLEN];
	int i;

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_record)) {

                gtk_tree_model_get(model, &iter_record, 0, &precord_name, -1);
                strncpy(record_name, precord_name, MAXLEN-1);
                g_free(precord_name);
		
		gtk_tree_model_iter_parent(model, &iter_artist, &iter_record);
		if (iter_artist.stamp != GTK_TREE_STORE(model)->stamp)
			return;
                gtk_tree_model_get(model, &iter_artist, 0, &partist_name, -1);
                strncpy(artist_name, partist_name, MAXLEN-1);
                g_free(partist_name);

		i = 0;
		while (gtk_tree_model_iter_nth_child(model, &iter_track, &iter_record, i++)) {

			gtk_tree_model_get(model, &iter_track, 0, &ptrack_name, 2, &pfile, -1);
			strncpy(track_name, ptrack_name, MAXLEN-1);
			strncpy(file, pfile, MAXLEN-1);
			g_free(ptrack_name);
			g_free(pfile);
			
			make_title_string(list_str, title_format,
					  artist_name, record_name, track_name);

			gtk_list_store_insert_before(play_store,
						     &list_iter,
						     (GtkTreeIter *)data);

			gtk_list_store_set(play_store, &list_iter, 0, list_str, 1, file,
					   2, pl_color_inactive, -1);
		}
	}
}


static void
record__add_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeIter parent_iter;
	GtkTreeIter child_iter;
	GtkTreePath * parent_path;
	GtkTreeModel * model;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char ** strings = NULL;
	char * str;
	char comment[MAXLEN];
	char str_n[16];

	int i;

	name[0] = '\0';
	sort_name[0] = '\0';
	comment[0] = '\0';

	if (gtk_tree_selection_get_selected(music_select, &model, &parent_iter)) {

		/* get iter to artist (parent) */
		parent_path = gtk_tree_model_get_path(model, &parent_iter);
		if (gtk_tree_path_get_depth(parent_path) == 2)
			gtk_tree_path_up(parent_path);
		gtk_tree_model_get_iter(model, &parent_iter, parent_path);
		
		if (add_record_dialog(name, sort_name, &strings, comment)) {
			
			gtk_tree_store_append(music_store, &iter, &parent_iter);
			gtk_tree_store_set(music_store, &iter, 0, name, 1, sort_name,
					   2, "", 3, comment, -1);

			if (strings) {
				for (i = 0; strings[i] != NULL; i++) {
					sprintf(str_n, "%02d", i+1);
					str = strings[i];
					while (strstr(str, "/"))
						str = strstr(str, "/") + 1;
					if (str) {
						gtk_tree_store_append(music_store, &child_iter, &iter);
						gtk_tree_store_set(music_store, &child_iter,
								   0, str, 1, str_n,
								   2, strings[i], 3, "", -1);
					}
					free(strings[i]);
				}
				free(strings);
			}
		}
	}
}


static void
record__edit_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	char * pname;
	char * psort_name;
	char * pcomment;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char comment[MAXLEN];
	
	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

                gtk_tree_model_get(model, &iter, 0, &pname, 1, &psort_name, 3, &pcomment, -1);

		strncpy(name, pname, MAXLEN-1);
		strncpy(sort_name, psort_name, MAXLEN-1);
		strncpy(comment, pcomment, MAXLEN-1);

                g_free(pname);
                g_free(psort_name);
                g_free(pcomment);

		if (edit_record_dialog(name, sort_name, comment)) {

			gtk_tree_store_set(music_store, &iter, 0, name, 1, sort_name, 2, "", 3, comment, -1);
			tree_selection_changed_cb(music_select, NULL);
		}
        }
}


static void
record__remove_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;
	char * pname;
	char name[MAXLEN];
	char text[MAXLEN];

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

                gtk_tree_model_get(model, &iter, 0, &pname, -1);
		strncpy(name, pname, MAXLEN-1);
                g_free(pname);
		
		snprintf(text, MAXLEN-1, _("Really remove \"%s\" from the Music Store?"), name);
		if (confirm_dialog(_("Remove Record"), text)) {
			gtk_tree_store_remove(music_store, &iter);
			gtk_tree_selection_unselect_all(music_select);
			tree_selection_changed_cb(music_select, NULL);
		}
	}
}


/************************************/


void
track__addlist_cb(gpointer data) {

        GtkTreeIter iter_artist;
        GtkTreeIter iter_record;
        GtkTreeIter iter_track;
	GtkTreeIter list_iter;
        GtkTreeModel * model;

        char * partist_name;
        char * precord_name;
        char * ptrack_name;
        char * pfile;

        char artist_name[MAXLEN];
        char record_name[MAXLEN];
        char track_name[MAXLEN];
        char file[MAXLEN];

	char list_str[MAXLEN];

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_track)) {

                gtk_tree_model_get(model, &iter_track, 0, &ptrack_name, 2, &pfile, -1);
                strncpy(track_name, ptrack_name, MAXLEN-1);
                strncpy(file, pfile, MAXLEN-1);
                g_free(ptrack_name);
                g_free(pfile);
		
		gtk_tree_model_iter_parent(model, &iter_record, &iter_track);
                gtk_tree_model_get(model, &iter_record, 0, &precord_name, -1);
                strncpy(record_name, precord_name, MAXLEN-1);
                g_free(precord_name);

		gtk_tree_model_iter_parent(model, &iter_artist, &iter_record);
                gtk_tree_model_get(model, &iter_artist, 0, &partist_name, -1);
                strncpy(artist_name, partist_name, MAXLEN-1);
                g_free(partist_name);

		make_title_string(list_str, title_format,
				  artist_name, record_name, track_name);

		gtk_list_store_insert_before(play_store,
					     &list_iter,
					     (GtkTreeIter *)data);

		gtk_list_store_set(play_store, &list_iter, 0, list_str, 1, file,
				   2, pl_color_inactive, -1);
	}
}


static void
track__add_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeIter parent_iter;
	GtkTreePath * parent_path;
	GtkTreeModel * model;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char file[MAXLEN];
	char comment[MAXLEN];

	name[0] = '\0';
	sort_name[0] = '\0';
	file[0] = '\0';
	comment[0] = '\0';

	if (gtk_tree_selection_get_selected(music_select, &model, &parent_iter)) {

		/* get iter to artist (parent) */
		parent_path = gtk_tree_model_get_path(model, &parent_iter);
		if (gtk_tree_path_get_depth(parent_path) == 3)
			gtk_tree_path_up(parent_path);
		gtk_tree_model_get_iter(model, &parent_iter, parent_path);
		
		if (add_track_dialog(name, sort_name, file, comment)) {
			
			gtk_tree_store_append(music_store, &iter, &parent_iter);
			gtk_tree_store_set(music_store, &iter, 0, name, 1, sort_name,
					   2, file, 3, comment, -1);
		}


	}
}


static void
track__edit_cb(gpointer data) {

        GtkTreeIter iter;
        GtkTreeModel * model;

        char * pname;
        char * psort_name;
	char * pfile;
        char * pcomment;

        char name[MAXLEN];
        char sort_name[MAXLEN];
	char file[MAXLEN];
        char comment[MAXLEN];

        if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

                gtk_tree_model_get(model, &iter, 0, &pname, 1, &psort_name, 2, &pfile, 3, &pcomment, -1);

                strncpy(name, pname, MAXLEN-1);
                strncpy(sort_name, psort_name, MAXLEN-1);
                strncpy(file, pfile, MAXLEN-1);
                strncpy(comment, pcomment, MAXLEN-1);

                g_free(pname);
                g_free(psort_name);
                g_free(pfile);
                g_free(pcomment);

                if (edit_track_dialog(name, sort_name, file, comment)) {

                        gtk_tree_store_set(music_store, &iter, 0, name, 1, sort_name,
					   2, file, 3, comment, -1);
                        tree_selection_changed_cb(music_select, NULL);
                }
        }
}


static void
track__remove_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;
	char * pname;
	char name[MAXLEN];
	char text[MAXLEN];

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

                gtk_tree_model_get(model, &iter, 0, &pname, -1);
		strncpy(name, pname, MAXLEN-1);
                g_free(pname);
		
		snprintf(text, MAXLEN-1, _("Really remove \"%s\" from the Music Store?"), name);
		if (confirm_dialog(_("Remove Track"), text)) {
			gtk_tree_store_remove(music_store, &iter);
			gtk_tree_selection_unselect_all(music_select);
			tree_selection_changed_cb(music_select, NULL);
		}
	}
}


/************************************/


static void
set_comment_text(char * str) {

	GtkTextBuffer * buffer;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
	gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), str, -1);
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
}


static void
tree_selection_changed_cb(GtkTreeSelection * selection, gpointer data) {

        GtkTreeIter iter;
        GtkTreeModel * model;
        gchar * comment;

        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {

                gtk_tree_model_get(model, &iter, 3, &comment, -1);
		if (comment[0] != '\0') {
			set_comment_text(comment);
		} else {
			set_comment_text(_("(no comment)"));
		}
                g_free(comment);

        } else {
		set_comment_text("");
	}
}


gint
row_collapsed_cb(GtkTreeView *view, GtkTreeIter * iter1, GtkTreePath * path1) {

        GtkTreeIter iter2;
        GtkTreeModel * model;

        if (!gtk_tree_selection_get_selected(music_select, &model, &iter2)) {
			set_comment_text("");
	}

	return FALSE;
}


void
browser_drag_begin(GtkWidget *widget, GdkDragContext * drag_context, gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {
		drag_info = gtk_tree_path_get_depth(gtk_tree_model_get_path(model, &iter));
	} else {
		drag_info = 0;
	}

	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(play_list), FALSE);
	gtk_drag_dest_set(play_list,
			  GTK_DEST_DEFAULT_ALL,
			  target_table,
			  1,
			  GDK_ACTION_COPY);
}


void
browser_drag_data_get(GtkWidget * widget, GdkDragContext * drag_context,
		      GtkSelectionData * data, guint info, guint time, gpointer user_data) {

	gtk_selection_data_set(data, data->target, 8, "\0", 1);
}


void
browser_drag_end(GtkWidget * widget, GdkDragContext * drag_context, gpointer user_data) {

	drag_info = 0;

	gtk_drag_dest_unset(play_list);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(play_list), TRUE);
}


void
create_music_browser(void) {

	GtkWidget * vpaned;

	GtkWidget * viewport1;
	GtkWidget * viewport2;
	GtkWidget * scrolled_win1;
	GtkWidget * scrolled_win2;

	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkTextBuffer * buffer;

	/* window creating stuff */
	browser_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(browser_window), _("Music Store"));
	gtk_window_set_gravity(GTK_WINDOW(browser_window), GDK_GRAVITY_STATIC);
	g_signal_connect(G_OBJECT(browser_window), "delete_event", G_CALLBACK(browser_window_close), NULL);
	g_signal_connect(G_OBJECT(browser_window), "key_press_event",
			 G_CALLBACK(browser_window_key_pressed), NULL);
	gtk_container_set_border_width(GTK_CONTAINER(browser_window), 2);
        gtk_widget_set_size_request(browser_window, 200, 300);

	vpaned = gtk_vpaned_new();
	gtk_paned_set_position(GTK_PANED(vpaned), 230);
        gtk_container_add(GTK_CONTAINER(browser_window), vpaned);

	/* create music store tree */
	if (!music_store) {
		music_store = gtk_tree_store_new(4,
						 G_TYPE_STRING,  /* visible name */
						 G_TYPE_STRING,  /* string to sort by */
						 G_TYPE_STRING,  /* physical filename (tracks only) */
						 G_TYPE_STRING); /* user comments */
	}

	music_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(music_store));
	gtk_widget_set_name(music_tree, "music_tree");

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Artist / Record / Track",
							  renderer,
							  "text", 0,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(music_tree), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(music_tree), FALSE);

	viewport1 = gtk_viewport_new(NULL, NULL);
	gtk_paned_pack1(GTK_PANED(vpaned), viewport1, TRUE, TRUE);

	scrolled_win1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrolled_win1, -1, 1);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win1),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(viewport1), scrolled_win1);

	gtk_container_add(GTK_CONTAINER(scrolled_win1), music_tree);

	music_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(music_tree));
	gtk_tree_selection_set_mode(music_select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(music_select), "changed", G_CALLBACK(tree_selection_changed_cb), NULL);
	g_signal_connect(G_OBJECT(music_tree), "row_collapsed", G_CALLBACK(row_collapsed_cb), NULL);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(music_store), 1, GTK_SORT_ASCENDING);

	/* setup drag-and-drop */
	gtk_drag_source_set(music_tree,
			    GDK_BUTTON1_MASK,
			    target_table,
			    1,
			    GDK_ACTION_COPY);

	gtk_drag_source_set_icon_pixbuf(music_tree,
					gdk_pixbuf_new_from_xpm_data((const char **)drag_xpm));

	g_signal_connect(G_OBJECT(music_tree), "drag_begin", G_CALLBACK(browser_drag_begin), NULL);
	g_signal_connect(G_OBJECT(music_tree), "drag_data_get", G_CALLBACK(browser_drag_data_get), NULL);
	g_signal_connect(G_OBJECT(music_tree), "drag_end", G_CALLBACK(browser_drag_end), NULL);

	/* create popup menu for blank space */
	blank_menu = gtk_menu_new();
	blank__add = gtk_menu_item_new_with_label(_("Add new artist..."));
	gtk_menu_shell_append(GTK_MENU_SHELL(blank_menu), blank__add);
	g_signal_connect_swapped(G_OBJECT(blank__add), "activate", G_CALLBACK(artist__add_cb), NULL);
	gtk_widget_show(blank__add);

	/* create popup menu for artist tree items */
	artist_menu = gtk_menu_new();
	artist__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	artist__separator1 = gtk_separator_menu_item_new();
	artist__add = gtk_menu_item_new_with_label(_("Add new artist..."));
	artist__edit = gtk_menu_item_new_with_label(_("Edit artist..."));
	artist__addrec = gtk_menu_item_new_with_label(_("Add new record to this artist..."));
	artist__separator2 = gtk_separator_menu_item_new();
	artist__remove = gtk_menu_item_new_with_label(_("Remove artist"));

	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__separator1);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__add);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__edit);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__addrec);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__remove);

	g_signal_connect_swapped(G_OBJECT(artist__addlist), "activate", G_CALLBACK(artist__addlist_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__add), "activate", G_CALLBACK(artist__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__edit), "activate", G_CALLBACK(artist__edit_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__addrec), "activate", G_CALLBACK(record__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__remove), "activate", G_CALLBACK(artist__remove_cb), NULL);

	gtk_widget_show(artist__addlist);
	gtk_widget_show(artist__separator1);
	gtk_widget_show(artist__add);
	gtk_widget_show(artist__edit);
	gtk_widget_show(artist__addrec);
	gtk_widget_show(artist__separator2);
	gtk_widget_show(artist__remove);

	/* create popup menu for record tree items */
	record_menu = gtk_menu_new();
	record__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	record__separator1 = gtk_separator_menu_item_new();
	record__add = gtk_menu_item_new_with_label(_("Add new record..."));
	record__edit = gtk_menu_item_new_with_label(_("Edit record..."));
	record__addtrk = gtk_menu_item_new_with_label(_("Add new track to this record..."));
	record__separator2 = gtk_separator_menu_item_new();
	record__remove = gtk_menu_item_new_with_label(_("Remove record"));

	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__separator1);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__add);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__edit);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__addtrk);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__remove);

	g_signal_connect_swapped(G_OBJECT(record__addlist), "activate", G_CALLBACK(record__addlist_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__add), "activate", G_CALLBACK(record__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__edit), "activate", G_CALLBACK(record__edit_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__addtrk), "activate", G_CALLBACK(track__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__remove), "activate", G_CALLBACK(record__remove_cb), NULL);

	gtk_widget_show(record__addlist);
	gtk_widget_show(record__separator1);
	gtk_widget_show(record__add);
	gtk_widget_show(record__edit);
	gtk_widget_show(record__addtrk);
	gtk_widget_show(record__separator2);
	gtk_widget_show(record__remove);

	/* create popup menu for track tree items */
	track_menu = gtk_menu_new();
	track__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	track__separator1 = gtk_separator_menu_item_new();
	track__add = gtk_menu_item_new_with_label(_("Add new track..."));
	track__edit = gtk_menu_item_new_with_label(_("Edit track..."));
	track__separator2 = gtk_separator_menu_item_new();
	track__remove = gtk_menu_item_new_with_label(_("Remove track"));

	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__separator1);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__add);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__edit);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__remove);

	g_signal_connect_swapped(G_OBJECT(track__addlist), "activate", G_CALLBACK(track__addlist_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__add), "activate", G_CALLBACK(track__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__edit), "activate", G_CALLBACK(track__edit_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__remove), "activate", G_CALLBACK(track__remove_cb), NULL);

	gtk_widget_show(track__addlist);
	gtk_widget_show(track__separator1);
	gtk_widget_show(track__add);
	gtk_widget_show(track__edit);
	gtk_widget_show(track__separator2);
	gtk_widget_show(track__remove);

	/* attach event handler that will popup the menus */
	g_signal_connect_swapped(G_OBJECT(music_tree), "event", G_CALLBACK(music_tree_event_cb), NULL);

	/* event handler for double-clicking */
	g_signal_connect(G_OBJECT(music_tree), "button_press_event", G_CALLBACK(dblclick_handler), NULL);

	/* create text widget for comments */
	comment_view = gtk_text_view_new();
	gtk_widget_set_name(comment_view, "comment_view");
	gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), FALSE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));

	scrolled_win2 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrolled_win2, -1, 1);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win2),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	viewport2 = gtk_viewport_new(NULL, NULL);

	gtk_paned_pack2(GTK_PANED(vpaned), viewport2, FALSE, TRUE);
	gtk_container_add(GTK_CONTAINER(viewport2), scrolled_win2);
	gtk_container_add(GTK_CONTAINER(scrolled_win2), comment_view);
}


void
show_music_browser(void) {

	browser_on = 1;
	gtk_widget_show_all(browser_window);
	gtk_window_move(GTK_WINDOW(browser_window), browser_pos_x, browser_pos_y);
	gtk_window_resize(GTK_WINDOW(browser_window), browser_size_x, browser_size_y);
}


void
hide_music_browser(void) {

	browser_on = 0;
	gtk_window_get_position(GTK_WINDOW(browser_window), &browser_pos_x, &browser_pos_y);
	gtk_window_get_size(GTK_WINDOW(browser_window), &browser_size_x, &browser_size_y);
	gtk_widget_hide(browser_window);
}


/*********************************************************************************/

void
parse_track(xmlDocPtr doc, xmlNodePtr cur, GtkTreeIter * iter_record) {

	GtkTreeIter iter_track;
	xmlChar * key;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char file[MAXLEN];
	char comment[MAXLEN];

	name[0] = '\0';
	sort_name[0] = '\0';
	file[0] = '\0';
	comment[0] = '\0';

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(name, key, MAXLEN-1);
			xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"sort_name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(sort_name, key, MAXLEN-1);
			xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"file"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(file, key, MAXLEN-1);
			xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"comment"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(comment, key, MAXLEN-1);
			xmlFree(key);
		}
		cur = cur->next;
	}

	if ((name[0] != '\0') && (file != '\0')) {
		gtk_tree_store_append(music_store, &iter_track, iter_record);
		gtk_tree_store_set(music_store, &iter_track,
				   0, name,
				   1, sort_name,
				   2, file,
				   3, comment,
				   -1);
	} else {
		if (name[0] == '\0')
			fprintf(stderr, "Error in XML music_store: Track <name> is required, but NULL\n");
		else if (file[0] == '\0')
			fprintf(stderr, "Error in XML music_store: Track <file> is required, but NULL\n");
	}

	return;
}


void
parse_record(xmlDocPtr doc, xmlNodePtr cur, GtkTreeIter * iter_artist) {

	GtkTreeIter iter_record;
	xmlChar * key;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char comment[MAXLEN];

	name[0] = '\0';
	sort_name[0] = '\0';
	comment[0] = '\0';

	cur = cur->xmlChildrenNode;
	gtk_tree_store_append(music_store, &iter_record, iter_artist);
	gtk_tree_store_set(music_store, &iter_record, 0, "", 1, "", 2, "", 3, "", -1);
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(name, key, MAXLEN-1);
			xmlFree(key);
			if (name[0] == '\0') {
				fprintf(stderr, "Error in XML music_store: "
				       "Record <name> is required, but NULL\n");
			}
			gtk_tree_store_set(music_store, &iter_record, 0, name, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"sort_name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(sort_name, key, MAXLEN-1);
			xmlFree(key);
			gtk_tree_store_set(music_store, &iter_record, 1, sort_name, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"comment"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(comment, key, MAXLEN-1);
			xmlFree(key);
			gtk_tree_store_set(music_store, &iter_record, 3, comment, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"track"))) {

			parse_track(doc, cur, &iter_record);
		}
		cur = cur->next;
	}

	return;
}


void
parse_artist(xmlDocPtr doc, xmlNodePtr cur) {

	GtkTreeIter iter_artist;
	xmlChar * key;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char comment[MAXLEN];

	name[0] = '\0';
	sort_name[0] = '\0';
	comment[0] = '\0';

	cur = cur->xmlChildrenNode;
	gtk_tree_store_append(music_store, &iter_artist, NULL);
	gtk_tree_store_set(music_store, &iter_artist, 0, "", 1, "", 2, "", 3, "", -1);
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(name, key, MAXLEN-1);
			xmlFree(key);
			if (name[0] == '\0') {
				fprintf(stderr, "Error in XML music_store: "
				       "Artist <name> is required, but NULL\n");
			}
			gtk_tree_store_set(music_store, &iter_artist, 0, name, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"sort_name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(sort_name, key, MAXLEN-1);
			xmlFree(key);
			gtk_tree_store_set(music_store, &iter_artist, 1, sort_name, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"comment"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(comment, key, MAXLEN-1);
			xmlFree(key);
			gtk_tree_store_set(music_store, &iter_artist, 3, comment, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"record"))) {

			parse_record(doc, cur, &iter_artist);
		}
		cur = cur->next;
	}

	return;
}


void
load_music_store(void) {

	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlNodePtr root;
	char store_file[MAXLEN];
	FILE * f;

	sprintf(store_file, "%s/music_store.xml", confdir);

	if ((f = fopen(store_file, "rt")) == NULL) {
		fprintf(stderr, "No music store found, creating empty one: %s\n", store_file);
		doc = xmlNewDoc("1.0");
		root = xmlNewNode(NULL, "music_store");
		xmlDocSetRootElement(doc, root);
		xmlSaveFormatFile(store_file, doc, 1);
		return;
	}
	fclose(f);
	
	doc = xmlParseFile(store_file);
	if (doc == NULL) {
		fprintf(stderr, "An XML error occured while parsing %s\n", store_file);
		return;
	}
	
	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		fprintf(stderr, "load_music_store: empty XML document\n");
		xmlFreeDoc(doc);
		return;
	}

	if (xmlStrcmp(cur->name, (const xmlChar *)"music_store")) {
		fprintf(stderr, "load_music_store: XML document of the wrong type, "
			"root node != music_store\n");
		xmlFreeDoc(doc);
		return;
	}
	
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"artist"))){
			parse_artist(doc, cur);
		}
		cur = cur->next;
	}
	
	xmlFreeDoc(doc);
	return;
}

/**********************************************************************************/


void
save_track(xmlDocPtr doc, xmlNodePtr node_track, GtkTreeIter * iter_track) {

	xmlNodePtr node;
	char * name;
	char * sort_name;
	char * file;
	char * comment;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_track,
			   0, &name, 1, &sort_name, 2, &file, 3, &comment, -1);
	
	node = xmlNewTextChild(node_track, NULL, "track", NULL);
	if (name[0] == '\0')
		fprintf(stderr, "saving music_store XML: warning: track node with empty <name>\n");
	xmlNewTextChild(node, NULL, "name", name);
	if (sort_name[0] != '\0')
		xmlNewTextChild(node, NULL, "sort_name", sort_name);
	if (file[0] == '\0')
		fprintf(stderr, "saving music_store XML: warning: track node with empty <file>\n");
	xmlNewTextChild(node, NULL, "file", file);
	if (comment[0] != '\0')
		xmlNewTextChild(node, NULL, "comment", comment);

	g_free(name);
	g_free(sort_name);
	g_free(file);
	g_free(comment);
}


void
save_record(xmlDocPtr doc, xmlNodePtr node_record, GtkTreeIter * iter_record) {

	xmlNodePtr node;
	char * name;
	char * sort_name;
	char * comment;
	GtkTreeIter iter_track;
	int i = 0;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_record,
			   0, &name, 1, &sort_name, 3, &comment, -1);
	
	node = xmlNewTextChild(node_record, NULL, "record", NULL);
	if (name[0] == '\0')
		fprintf(stderr, "saving music_store XML: warning: record node with empty <name>\n");
	xmlNewTextChild(node, NULL, "name", name);
	if (sort_name[0] != '\0')
		xmlNewTextChild(node, NULL, "sort_name", sort_name);
	if (comment[0] != '\0')
		xmlNewTextChild(node, NULL, "comment", comment);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track, iter_record, i++))
		save_track(doc, node, &iter_track);

	g_free(name);
	g_free(sort_name);
	g_free(comment);
}


void
save_artist(xmlDocPtr doc, xmlNodePtr root, GtkTreeIter * iter_artist) {

	xmlNodePtr node;
	char * name;
	char * sort_name;
	char * comment;
	GtkTreeIter iter_record;
	int i = 0;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_artist,
			   0, &name, 1, &sort_name, 3, &comment, -1);
	
	node = xmlNewTextChild(root, NULL, "artist", NULL);
	if (name[0] == '\0')
		fprintf(stderr, "saving music_store XML: warning: artist node with empty <name>\n");
	xmlNewTextChild(node, NULL, "name", name);
	if (sort_name[0] != '\0')
		xmlNewTextChild(node, NULL, "sort_name", sort_name);
	if (comment[0] != '\0')
		xmlNewTextChild(node, NULL, "comment", comment);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_record, iter_artist, i++))
		save_record(doc, node, &iter_record);

	g_free(name);
	g_free(sort_name);
	g_free(comment);
}


void
save_music_store(void) {

	xmlDocPtr doc;
	xmlNodePtr root;
	GtkTreeIter iter_artist;
	int i = 0;
	char c, d;
	FILE * fin;
	FILE * fout;
	char tmpname[MAXLEN];
	char store_file[MAXLEN];


	sprintf(store_file, "%s/music_store.xml", confdir);

	doc = xmlNewDoc("1.0");
	root = xmlNewNode(NULL, "music_store");
	xmlDocSetRootElement(doc, root);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_artist, NULL, i++))
		save_artist(doc, root, &iter_artist);


	sprintf(tmpname, "%s/music_store.xml.temp", confdir);
	xmlSaveFormatFile(tmpname, doc, 1);
	
	if ((fin = fopen(store_file, "rt")) == NULL) {
		fprintf(stderr, "Error opening file: %s\n", store_file);
		return;
	}
	if ((fout = fopen(tmpname, "rt")) == NULL) {
		fprintf(stderr, "Error opening file: %s\n", tmpname);
		return;
	}
	
	c = 0; d = 0;
	while (((c = fgetc(fin)) != EOF) && ((d = fgetc(fout)) != EOF)) {
		if (c != d) {
			fclose(fin);
			fclose(fout);
			unlink(store_file);
			rename(tmpname, store_file);
			return;
		}
	}
	
	fclose(fin);
	fclose(fout);
	unlink(tmpname);
}