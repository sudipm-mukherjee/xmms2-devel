/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

#include <xmmsclient/xmmsclient.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "main.h"
#include "status.h"
#include "utils.h"
#include "readline.h"
#include "cli_cache.h"
#include "cli_infos.h"

static void
status_update_playback (cli_infos_t *infos, status_entry_t *entry)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	guint status;
	gchar *playback;
	const gchar *err;

	res = xmmsc_playback_status (infos->sync);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		xmmsv_get_uint (val, &status);

		switch (status) {
		case XMMS_PLAYBACK_STATUS_STOP:
			playback = g_strdup (_("Stopped"));
			break;
		case XMMS_PLAYBACK_STATUS_PLAY:
			playback = g_strdup (_("Playing"));
			break;
		case XMMS_PLAYBACK_STATUS_PAUSE:
			playback = g_strdup (_("Paused"));
			break;
		default:
			playback = g_strdup (_("Unknown"));
		}

		g_hash_table_insert (entry->data, "playback_status", playback);

	} else {
		g_printf (_("Server error: %s\n"), err);
	}

	xmmsc_result_unref (res);
}

static void
status_update_info (cli_infos_t *infos, status_entry_t *entry)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	guint currid;
	gint i;

	const gchar *time_fields[] = { "duration", NULL };
	const gchar *noinfo_fields[] = { "playback_status", "playtime", "position", NULL };
	const gchar *err;

	currid = infos->cache->currid;

	res = xmmsc_medialib_get_info (infos->sync, currid);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		GList *it;
		xmmsv_t *info;

		info = xmmsv_propdict_to_dict (val, NULL);

		for (it = g_list_first (entry->format);
		     it != NULL;
		     it = g_list_next (it)) {

			gint ival;
			gchar *value, *field;
			const gchar *sval;
			xmmsv_type_t type;

			field = (gchar *)it->data;
			if (field[0] != '$' || field[1] != '{') {
				continue;
			}

			field += 2;
			for (i = 0; noinfo_fields[i] != NULL; i++) {
				if (!strcmp (field, noinfo_fields[i])) {
					goto not_info_field;
				}
			}

			type = xmmsv_dict_entry_get_type (info, field);
			switch (type) {
			case XMMSV_TYPE_NONE:
				value = NULL;
				if (!strcmp (field, "title")) {
					if (xmmsv_dict_entry_get_string (info,
					                                 "url",
					                                 &sval)) {
						value = g_path_get_basename (sval);
					}
				}
				if (!value) {
					value = g_strdup ("Unknown");
				}
				break;

			case XMMSV_TYPE_STRING:
				xmmsv_dict_entry_get_string (info, field, &sval);
				value = g_strdup (sval);
				break;

			case XMMSV_TYPE_INT32:
				xmmsv_dict_entry_get_int (info, field, &ival);

				for (i = 0; time_fields[i] != NULL; i++) {
					if (!strcmp (time_fields[i], field)) {
						break;
					}
				}

				if (time_fields[i] != NULL) {
					value = format_time (ival, FALSE);
				} else {
					value = g_strdup_printf ("%d", ival);
				}
				break;

			default:
				value = g_strdup (_("invalid field"));
			}

			/* FIXME: work with undefined fileds! pll parser? */
			g_hash_table_insert (entry->data, field, value);

		    not_info_field:
			;
		}

		xmmsv_unref (info);

	} else {
		g_printf (_("Server error: %s\n"), err);
	}

err:

	xmmsc_result_unref (res);
}

static void
status_update_playtime (cli_infos_t *infos, status_entry_t *entry)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	guint playtime;
	const gchar *err;

	res = xmmsc_playback_playtime (infos->sync);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		xmmsv_get_uint (val, &playtime);
		g_hash_table_insert (entry->data, "playtime", format_time (playtime, FALSE));
	} else {
		g_printf (_("Server error: %s\n"), err);
	}

	xmmsc_result_unref (res);
}

static void
status_update_position (cli_infos_t *infos, status_entry_t *entry)
{
	g_hash_table_insert (entry->data, "position",
	                     g_strdup_printf ("%d", infos->cache->currpos));
}

static GList *
parse_format (const gchar *format)
{
	const gchar *s, *last;
	GList *strings = NULL;

	last = format;
	while ((s = strstr (last, "${")) != NULL) {
		/* Copy the substring before the variable */
		if (last != s) {
			strings = g_list_prepend (strings, g_strndup (last, s - last));
		}

		last = strchr (s, '}');
		if (last) {
			/* Copy the variable (as "${variable}") */
			strings = g_list_prepend (strings, g_strndup (s, last - s));
			last++;
		} else {
			/* Missing '}', keep '$' as a string and keep going */
			strings = g_list_prepend (strings, g_strndup (s, 1));
			last = s + 1;
		}
	}

	/* Copy the remaining substring after the last variable */
	if (*last != '\0') {
		strings = g_list_prepend (strings, g_strdup (last));
	}

	strings = g_list_reverse (strings);

	return strings;
}

status_entry_t *
status_init (gchar *format, gint refresh)
{
	status_entry_t *entry;

	entry = g_new0 (status_entry_t, 1);

	entry->data = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
	entry->format = parse_format (format);
	entry->refresh = refresh;

	return entry;
}

void
status_free (status_entry_t *entry)
{
	GList *it;

	for (it = g_list_first (entry->format); it != NULL; it = g_list_next (it)) {
		g_free (it->data);
	}
	g_list_free (entry->format);

	g_hash_table_destroy (entry->data);
	g_free (entry);
}

void
status_update_all (cli_infos_t *infos, status_entry_t *entry)
{
	status_update_playback (infos, entry);
	status_update_position (infos, entry);
	status_update_info (infos, entry);
	status_update_playtime (infos, entry);
}

void
status_print_entry (status_entry_t *entry)
{
	GList *it;
	gint columns, currlen;

	columns = find_terminal_width ();

	currlen = 0;
	g_printf ("\r");
	for (it = g_list_first (entry->format); it != NULL; it = g_list_next (it)) {
		gchar *s = it->data;
		if (s[0] == '$' && s[1] == '{') {
			s = g_hash_table_lookup (entry->data, s+2);
		}

		/* FIXME: print ellipsis when len > columns */
		currlen += g_utf8_strlen (s, -1);
		if (currlen >= columns) {
			break;
		} else {
			g_printf ("%s", s);
		}
	}

	while (currlen++ < columns) {
		g_printf (" ");
	}

	fflush (stdout);
}
