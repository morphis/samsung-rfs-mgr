/*
 *  samsung-modem-mgr
 *
 *  Copyright (C) 2012  Simon Busch. All rights reserved.
 *
 *  Some parts of the code are fairly copied from the ofono project under
 *  the terms of the GPLv2.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/signalfd.h>

#include <glib.h>

#include "rfs.h"

#define SHUTDOWN_GRACE_SECONDS 2

static GMainLoop *event_loop;
static struct rfs_manager *rfs = NULL;

void __samsung_modem_mgr_exit(void)
{
	g_main_loop_quit(event_loop);
}

static gboolean quit_eventloop(gpointer user_data)
{
	__samsung_modem_mgr_exit();
	return FALSE;
}

static unsigned int __terminated = 0;

static gboolean signal_handler(GIOChannel *channel, GIOCondition cond,
							gpointer user_data)
{
	struct signalfd_siginfo si;
	ssize_t result;
	int fd;

	if (cond & (G_IO_NVAL | G_IO_ERR | G_IO_HUP))
		return FALSE;

	fd = g_io_channel_unix_get_fd(channel);

	result = read(fd, &si, sizeof(si));
	if (result != sizeof(si))
		return FALSE;

	switch (si.ssi_signo) {
	case SIGINT:
	case SIGTERM:
		if (__terminated == 0) {
			g_message("Terminating");
			g_timeout_add_seconds(SHUTDOWN_GRACE_SECONDS,
						quit_eventloop, NULL);
		}

		__terminated = 1;
		break;
	}

	return TRUE;
}

static guint setup_signalfd(void)
{
	GIOChannel *channel;
	guint source;
	sigset_t mask;
	int fd;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
		perror("Failed to set signal mask");
		return 0;
	}

	fd = signalfd(-1, &mask, 0);
	if (fd < 0) {
		perror("Failed to create signal descriptor");
		return 0;
	}

	channel = g_io_channel_unix_new(fd);

	g_io_channel_set_close_on_unref(channel, TRUE);
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);

	source = g_io_add_watch(channel,
				G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				signal_handler, NULL);

	g_io_channel_unref(channel);

	return source;
}

static gboolean option_detach = FALSE;
static gboolean option_version = FALSE;
static gboolean option_debug = FALSE;

static GOptionEntry options[] = {
	{ "nodetach", 'n', G_OPTION_FLAG_REVERSE,
				G_OPTION_ARG_NONE, &option_detach,
				"Don't run as daemon in background" },
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &option_version,
				"Show version information and exit" },
	{ "debug", 'd', G_OPTION_FLAG_REVERSE,
				G_OPTION_ARG_NONE, &option_debug,
				"Output debug information" },
	{ NULL },
};

static void debug_handler(const gchar *log_domain, GLogLevelFlags log_level,
						const gchar *message, gpointer user_data)
{
	g_print("DEBUG: %s\n", message);
}

int main(int argc, char **argv)
{
	GOptionContext *context;
	GError *err = NULL;
	guint signal;
	int rc;

	g_message("Samsung RFS Manager %s", VERSION);

	g_log_set_handler (NULL, G_LOG_LEVEL_DEBUG, debug_handler, NULL);

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, options, NULL);

	if (g_option_context_parse(context, &argc, &argv, &err) == FALSE) {
		if (err != NULL) {
			g_printerr("%s\n", err->message);
			g_error_free(err);
			exit(1);
		}

		g_printerr("An unknown error occurred\n");
		exit(1);
	}

	g_option_context_free(context);

	if (option_version == TRUE) {
		printf("%s\n", VERSION);
		exit(0);
	}

	if (option_detach == TRUE) {
		if (daemon(0, 0)) {
			perror("Can't start daemon");
			return 1;
		}
	}

	event_loop = g_main_loop_new(NULL, FALSE);

	signal = setup_signalfd();

	rfs = rfs_manager_new();
	if (!rfs) {
		g_printerr("Failed to create the RFS manager");
		goto cleanup;
	}

	if (rfs_manager_start(rfs) < 0) {
		g_printerr("Failed to start the RFS manager; shutting down ...");
		goto cleanup;
	}

	g_main_loop_run(event_loop);

cleanup:
	if (rfs) {
		if (rfs_manager_is_running(rfs))
			rfs_manager_stop(rfs);
		rfs_manager_free(rfs);
	}

	g_source_remove(signal);

	g_main_loop_unref(event_loop);

	return 0;
}

// vim:ts=4:sw=4:noexpandtab
