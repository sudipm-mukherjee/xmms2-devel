/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */
#include "cmd_pls.h"
#include "common.h"

extern gchar *listformat;

static void
add_item_to_playlist (xmmsc_connection_t *conn, gchar *playlist, gchar *item)
{
	xmmsc_result_t *res;
	gchar *url;

	url = format_url (item);
	if (!url) {
		print_error ("Invalid url");
	}

	res = xmmsc_playlist_add_url (conn, playlist, url);
	xmmsc_result_wait (res);
	g_free (url);

	if (xmmsc_result_iserror (res)) {
		print_error ("Couldn't add %s to playlist: %s\n", item,
		             xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

	print_info ("Added %s", item);
}


static void
add_directory_to_playlist (xmmsc_connection_t *conn, gchar *playlist,
                           gchar *directory, gboolean recursive)
{
	GSList *entries = NULL;
	const gchar *entry;
	gchar *buf;
	GDir *dir;

	dir = g_dir_open (directory, 0, NULL);
	if (!dir) {
		print_error ("cannot open directory: %s", directory);
	}

	while ((entry = g_dir_read_name (dir))) {
		entries = g_slist_prepend (entries, g_strdup (entry));
	}
	g_dir_close (dir);

	/* g_dir_read_name() will return the entries in a undefined
	 * order, so sort the list now.
	 */
	entries = g_slist_sort (entries, (GCompareFunc) strcmp);

	while (entries) {
		buf = g_build_path (G_DIR_SEPARATOR_S, directory, 
		                    entries->data, NULL);

		if (g_file_test (buf, G_FILE_TEST_IS_DIR)) {
			if (recursive) {
				add_directory_to_playlist (conn, playlist, buf, recursive);
			}
		} else {
			add_item_to_playlist (conn, playlist, buf);
		}

		g_free (buf);
		g_free (entries->data);
		entries = g_slist_delete_link (entries, entries);
	}
}


void
cmd_addid (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gint i;
	gchar *playlist = NULL;
	xmmsc_result_t *res;

	if (argc < 3) {
		print_error ("Need a medialib id to add");
	}

	for (i = 2; argv[i]; i++) {
		guint id = strtoul (argv[i], NULL, 10);
		if (id) {
			res = xmmsc_playlist_add_id (conn, playlist, id);
			xmmsc_result_wait (res);

			if (xmmsc_result_iserror (res)) {
				print_error ("Couldn't add %d to playlist: %s", id, 
							 xmmsc_result_get_error (res));
			}
			xmmsc_result_unref (res);

			print_info ("Added medialib id %d to playlist", id);
		} else if (i == 2) {
			/* First argument is the playlist name */
			playlist = argv[i];
		}
	}
}


void
cmd_addpls (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gint i;

	if (argc < 3) {
		print_error ("Need a playlist url to add");
	}

	for (i = 2; argv[i]; i++) {
		xmmsc_result_t *res;
		gchar *url;

		url = format_url (argv[i]);
		if (!url) {
			print_error ("Invalid url");
		}

		res = xmmsc_playlist_import (conn, "_xmms2cli", url);
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}
		xmmsc_result_unref (res);

		/* FIXME: Hack is BROKEN now ! use add_coll instead ! */
		res = xmmsc_playlist_load (conn, "_xmms2cli");
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}
		xmmsc_result_unref (res);

		print_info ("Added playlist %s", url);
		g_free (url);
	}
}


void
cmd_add (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	gint i;

	if (argc < 3) {
		print_error ("Need a filename to add");
	}

	for (i = 2; argv[i]; i++) {
		/* FIXME: Fulhack to check for optional playlist argument */
		if (i == 2 && !g_file_test (argv[i], G_FILE_TEST_EXISTS)) {
			playlist = argv[i];
		}

		add_item_to_playlist (conn, playlist, argv[i]);
	}
}

void
cmd_addarg (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gchar *playlist = NULL;
	gchar *url;
	const gchar *arg_start;

	if (argc < 4) {
		print_error ("Need a filename and args to add");
	}

	url = format_url (argv[2]);
	if (!url) {
		url = format_url (argv[3]);
		if (!url) {
			print_error ("Invalid url");
		} else {
			/* FIXME: Fulhack to check for optional playlist argument */
			playlist = argv[2];
			arg_start = argv[4];
		}
	} else {
		arg_start = argv[3];
	}

	res = xmmsc_playlist_add_args (conn, playlist, url, argc - 3, &arg_start);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("Couldn't add %s to playlist: %s\n", url,
		             xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

	print_info ("Added %s", url);

	g_free (url);
}


void
cmd_radd (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	gint i;

	if (argc < 3) {
		print_error ("Need a directory to add");
	}

	for (i = 2; argv[i]; i++) {
		if (i == 2 && !g_file_test (argv[i], G_FILE_TEST_EXISTS)) {
			playlist = argv[i];
		} else if (!g_file_test (argv[i], G_FILE_TEST_IS_DIR)) {
			print_info ("not a directory: %s", argv[i]);
		} else {
			add_directory_to_playlist (conn, playlist, argv[i], TRUE);
		}
	}
}


void
cmd_clear (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	xmmsc_result_t *res;

	if (argc == 3) {
		playlist = argv[2];
	}

	res = xmmsc_playlist_clear (conn, playlist);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}


void
cmd_shuffle (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	xmmsc_result_t *res;

	if (argc == 3) {
		playlist = argv[2];
	}
	
	res = xmmsc_playlist_shuffle (conn, playlist);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}


void
cmd_sort (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist;
	const gchar *sortby;
	xmmsc_result_t *res;
	
	if (argc < 3) {
		print_error ("Sort needs a property to sort on, %d", argc);
	} else if (argc == 3) {
		playlist = NULL;
		sortby = argv[2];
	} else {
		playlist = argv[2];
		sortby = argv[3];
	}
	
	res = xmmsc_playlist_sort (conn, playlist, &sortby);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}


gint
cmp (const void *av, const void *bv)
{
	gint result;
	gint a = *(gint *) av;
	gint b = *(gint *) bv;

	result = (a > b ? -1 : 1);

	if (a == b) {
		result = 0;
	}

	return result;
}


void
cmd_remove (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	gint i, size = 0;
	gint *sort;

	if (argc < 3) {
		print_error ("Remove needs a position to be removed");
	}

	sort = g_malloc (sizeof (gint) * argc);

	for (i = 2; i < argc; i++) {
		gchar *endptr = NULL;
		sort[size] = strtol (argv[i], &endptr, 10);
		if (endptr != argv[i]) {
			size++;
		} else if (i == 2) {
			/* First argument is the playlist name */
			playlist = argv[i];
		}
	}

	qsort (sort, size, sizeof (gint), &cmp);

	for (i = 0; i < size; i++) {
		gint pos = sort[i];

		xmmsc_result_t *res = xmmsc_playlist_remove_entry (conn, playlist, pos);
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("Couldn't remove %d (%s)", pos,
			             xmmsc_result_get_error (res));
		}
		xmmsc_result_unref (res);
	}
	
	g_free (sort);
}


void
cmd_list (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	xmmsc_result_t *res;
	gulong total_playtime = 0;
	guint p = 0;
	guint pos = 0;

	if (argc > 2) {
		playlist = argv[2];
	}

	res = xmmsc_playlist_current_pos (conn);
	xmmsc_result_wait (res);

	if (!xmmsc_result_iserror (res)) {
		if (!xmmsc_result_get_uint (res, &p)) {
			print_error ("Broken resultset");
		}
		xmmsc_result_unref (res);
	}

	res = xmmsc_playlist_list_entries (conn, playlist);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	while (xmmsc_result_list_valid (res)) {
		xmmsc_result_t *info_res;
		gchar line[80];
		gint playtime = 0;
		guint ui;

		if (!xmmsc_result_get_uint (res, &ui)) {
			print_error ("Broken resultset");
		}

		info_res = xmmsc_medialib_get_info (conn, ui);
		xmmsc_result_wait (info_res);

		if (xmmsc_result_iserror (info_res)) {
			print_error ("%s", xmmsc_result_get_error (info_res));
		}

		if (xmmsc_result_get_dict_entry_int (info_res, "duration", &playtime)) {
			total_playtime += playtime;
		}
		
		if (res_has_key (info_res, "channel")) {
			if (res_has_key (info_res, "title")) {
				xmmsc_entry_format (line, sizeof (line),
				                    "[stream] ${title}", info_res);
			} else {
				xmmsc_entry_format (line, sizeof (line),
				                    "${channel}", info_res);
			}
		} else if (!res_has_key (info_res, "title")) {
			gchar *url, *filename;
		  	gchar dur[10];
			
			xmmsc_entry_format (dur, sizeof (dur),
			                    "(${minutes}:${seconds})", info_res);
			
			if (xmmsc_result_get_dict_entry_string (info_res, "url", &url)) {
				filename = g_path_get_basename (url);
				if (filename) {
					g_snprintf (line, sizeof (line), "%s %s", filename, dur);
					g_free (filename);
				} else {
					g_snprintf (line, sizeof (line), "%s %s", url, dur);
				}
			}
		} else {
			xmmsc_entry_format (line, sizeof(line), listformat, info_res);
		}

		if (p == pos) {
			print_info ("->[%d/%d] %s", pos, ui, line);
		} else {
			print_info ("  [%d/%d] %s", pos, ui, line);
		}

		pos++;

		xmmsc_result_unref (info_res);
		xmmsc_result_list_next (res);
	}
	xmmsc_result_unref (res);

	/* rounding */
	total_playtime += 500;

	print_info ("\nTotal playtime: %d:%02d:%02d", total_playtime / 3600000, 
	            (total_playtime / 60000) % 60, (total_playtime / 1000) % 60);
}


void
cmd_move (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	guint cur_pos, new_pos, arg_start;
	gchar *playlist;

	if (argc < 4) {
		print_error ("You'll need to specifiy current and new position");
	}

	if (argc == 4) {
		playlist = NULL;
		arg_start = 2;
	} else {
		playlist = argv[2];
		arg_start = 3;
	}

	cur_pos = strtol (argv[arg_start], NULL, 10);
	new_pos = strtol (argv[arg_start + 1], NULL, 10);

	res = xmmsc_playlist_move_entry (conn, playlist, cur_pos, new_pos);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("Unable to move playlist entry: %s",
		             xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

	print_info ("Moved %u to %u", cur_pos, new_pos);
}


void
cmd_playlist_load (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;

	if (argc < 3) {
		print_error ("Supply a playlist name");
	}

	res = xmmsc_playlist_load (conn, argv[2]);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}


void
cmd_playlist_create (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gchar *playlist_name;
	xmmsc_coll_t *playlist_coll;

	if (argc < 3) {
		print_error ("Supply a playlist name");
	}

	playlist_name = argv[2];
	playlist_coll = xmmsc_coll_new (XMMS_COLLECTION_TYPE_IDLIST);

	res = xmmsc_coll_save (conn, playlist_coll, playlist_name,
	                       XMMS_COLLECTION_NS_PLAYLISTS);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}


void
cmd_playlists_list (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *active_name;
	xmmsc_result_t *res, *active_res;

	active_res = xmmsc_playlist_current_active (conn);
	xmmsc_result_wait (active_res);

	if (xmmsc_result_iserror (active_res) || 
	    !xmmsc_result_get_string (active_res, &active_name)) {
		active_name = NULL;
	}

	res = xmmsc_playlist_list (conn);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	while (xmmsc_result_list_valid (res)) {
		gchar *name;

		if (!xmmsc_result_get_string (res, &name)) {
			print_error ("Broken resultset");
		}

		/* Hide all lists that start with _ */
		if (name[0] != '_') {
			if (active_name != NULL && strcmp (active_name, name) == 0) {
				print_info ("->%s", name);
			} else {
				print_info ("  %s", name);
			}
		}
		xmmsc_result_list_next (res);
	}
	xmmsc_result_unref (res);
	xmmsc_result_unref (active_res);
}


void
cmd_playlist_import (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gchar *url;

	if (argc < 4) {
		print_error ("Supply a playlist name and url");
	}

	url = format_url (argv[3]);
	if (!url) {
		print_error ("Invalid url");
	}

	res = xmmsc_playlist_import (conn, argv[2], url);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

	print_info ("Playlist imported");
}


void
cmd_playlist_remove (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;

	if (argc < 3) {
		print_error ("Supply a playlist name");
	}

	res = xmmsc_playlist_remove (conn, argv[2]);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

	print_info ("Playlist removed");
}


void
cmd_playlist_export (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gchar *file;
	gchar *mime;

	if (argc < 4) {
		print_error ("Supply a playlist name and a mimetype");
	}

	if (strcasecmp (argv[3], "m3u") == 0) {
		mime = "audio/mpegurl";
	} else if (strcasecmp (argv[3], "pls") == 0) {
		mime = "audio/x-scpls";
	} else if (strcasecmp (argv[3], "html") == 0) {
		mime = "text/html";
	} else {
		mime = argv[3];
	}

	res = xmmsc_playlist_export (conn, argv[2], mime);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	if (!xmmsc_result_get_string (res, &file)) {
		print_error ("Broken resultset!");
	}

	fwrite (file, strlen (file), 1, stdout);
	print_info ("Playlist exported");

	xmmsc_result_unref (res);
}
