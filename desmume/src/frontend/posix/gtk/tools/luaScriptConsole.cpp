/* luaScriptConsole.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2025 DeSmuME Team
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_LUA

#include <gtk/gtk.h>
#include <map>

#include "lua-engine.h"

struct LuaConsole {
	int uid;
	GtkWidget *window;
	GtkWidget *path_entry;
	GtkWidget *run_btn;
	GtkWidget *stop_btn;
	GtkWidget *output_view;
	GtkTextBuffer *output_buf;
};

static std::map<int, LuaConsole*> g_consoles;
static int g_next_uid = 1;

static void console_append(LuaConsole *con, const char *str)
{
	GtkTextIter end;
	gtk_text_buffer_get_end_iter(con->output_buf, &end);
	gtk_text_buffer_insert(con->output_buf, &end, str, -1);

	GtkTextMark *mark = gtk_text_buffer_get_mark(con->output_buf, "insert");
	gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(con->output_view), mark);
}

static void lua_print_cb(int uid, const char *str)
{
	auto it = g_consoles.find(uid);
	if (it != g_consoles.end())
		console_append(it->second, str);
}

static void lua_onstart_cb(int uid)
{
	auto it = g_consoles.find(uid);
	if (it == g_consoles.end()) return;
	LuaConsole *con = it->second;

	gtk_text_buffer_set_text(con->output_buf, "", -1);
	gtk_button_set_label(GTK_BUTTON(con->run_btn), "Restart");
	gtk_widget_set_sensitive(con->run_btn, TRUE);
	gtk_widget_set_sensitive(con->stop_btn, TRUE);
}

static void lua_onstop_cb(int uid, bool statusOK)
{
	auto it = g_consoles.find(uid);
	if (it == g_consoles.end()) return;
	LuaConsole *con = it->second;

	gtk_button_set_label(GTK_BUTTON(con->run_btn), "Run");
	gtk_widget_set_sensitive(con->stop_btn, FALSE);

	if (!statusOK)
		console_append(con, "Script stopped with error.\n");
	else
		console_append(con, "Script finished.\n");
}

static void on_browse_clicked(GtkButton *btn, gpointer user_data)
{
	LuaConsole *con = (LuaConsole*)user_data;

	GtkFileFilter *filter_lua = gtk_file_filter_new();
	gtk_file_filter_set_name(filter_lua, "Lua scripts (*.lua)");
	gtk_file_filter_add_pattern(filter_lua, "*.lua");

	GtkFileFilter *filter_all = gtk_file_filter_new();
	gtk_file_filter_set_name(filter_all, "All files");
	gtk_file_filter_add_pattern(filter_all, "*");

	GtkFileChooserNative *fc = gtk_file_chooser_native_new(
		"Open Lua Script",
		GTK_WINDOW(con->window),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		"_Open", "_Cancel");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), filter_lua);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), filter_all);

	if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
		GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(fc));
		gchar *path = g_file_get_path(file);
		gtk_entry_set_text(GTK_ENTRY(con->path_entry), path);
		g_free(path);
		g_object_unref(file);
	}

	g_object_unref(fc);
}

static void on_run_clicked(GtkButton *btn, gpointer user_data)
{
	LuaConsole *con = (LuaConsole*)user_data;
	const gchar *path = gtk_entry_get_text(GTK_ENTRY(con->path_entry));
	if (!path || path[0] == '\0') return;

	StopLuaScript(con->uid);
	RunLuaScriptFile(con->uid, path);
}

static void on_stop_clicked(GtkButton *btn, gpointer user_data)
{
	LuaConsole *con = (LuaConsole*)user_data;
	StopLuaScript(con->uid);
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	LuaConsole *con = (LuaConsole*)user_data;
	StopLuaScript(con->uid);
	CloseLuaContext(con->uid);
	g_consoles.erase(con->uid);
	delete con;
	return FALSE;
}

void lua_script_open_console(GtkWindow *parent)
{
	LuaConsole *con = new LuaConsole();
	con->uid = g_next_uid++;

	con->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(con->window), "Lua Script Console");
	gtk_window_set_default_size(GTK_WINDOW(con->window), 480, 320);
	if (parent)
		gtk_window_set_transient_for(GTK_WINDOW(con->window), parent);

	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
	gtk_container_add(GTK_CONTAINER(con->window), vbox);

	/* Path row */
	GtkWidget *path_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	con->path_entry = gtk_entry_new();
	gtk_widget_set_hexpand(con->path_entry, TRUE);
	GtkWidget *browse_btn = gtk_button_new_with_label("Browse...");
	gtk_box_pack_start(GTK_BOX(path_row), con->path_entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(path_row), browse_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), path_row, FALSE, FALSE, 0);

	/* Button row */
	GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	con->run_btn = gtk_button_new_with_label("Run");
	con->stop_btn = gtk_button_new_with_label("Stop");
	gtk_widget_set_sensitive(con->stop_btn, FALSE);
	gtk_box_pack_start(GTK_BOX(btn_row), con->run_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(btn_row), con->stop_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), btn_row, FALSE, FALSE, 0);

	/* Output console */
	GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	con->output_view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(con->output_view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(con->output_view), GTK_WRAP_WORD_CHAR);

	GtkCssProvider *css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(css, "textview { font-family: monospace; }", -1, NULL);
	gtk_style_context_add_provider(
		gtk_widget_get_style_context(con->output_view),
		GTK_STYLE_PROVIDER(css),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(css);

	con->output_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(con->output_view));
	gtk_container_add(GTK_CONTAINER(scroll), con->output_view);
	gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

	/* Connect signals */
	g_signal_connect(browse_btn,  "clicked",      G_CALLBACK(on_browse_clicked), con);
	g_signal_connect(con->run_btn,  "clicked",    G_CALLBACK(on_run_clicked),    con);
	g_signal_connect(con->stop_btn, "clicked",    G_CALLBACK(on_stop_clicked),   con);
	g_signal_connect(con->window,   "delete-event", G_CALLBACK(on_window_delete), con);

	/* Register Lua context before showing the window */
	OpenLuaContext(con->uid, lua_print_cb, lua_onstart_cb, lua_onstop_cb);
	g_consoles[con->uid] = con;

	gtk_widget_show_all(con->window);
}

#endif /* HAVE_LUA */
