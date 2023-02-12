/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include "respondd-common.h"
#include <respondd.h>
#include <json-c/json.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

struct json_object * run_safe(const char * filename, const char * name, bool foreground) {
	FILE *fp;
	FILE *fo;
	int fd;

	char exec[40];
	sprintf(exec, "%s %s", "/usr/bin/olsr-respondd", name);

	fo = fopen(filename, "a");
	if (!fo) {
		fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
	} else {
		fd = fileno(fo);
		if (lockf(fd, F_TLOCK, 0)) {
			if (errno != EAGAIN) {
				fprintf(stderr, "Failed to lock %s: %s\n", filename, strerror(errno));
			}
			goto fo_fail;
		}

		goto fo_continue;
	}

fo_fail:
	fclose(fo);
	fo = NULL;
	if (!foreground) {
		return NULL;
	}

fo_continue:
	fp = popen(exec, "r");
	if (fp == NULL) {
		return NULL;
	}

	json_object * root = json_object_from_fd(fileno(fp));

	if (fo && root) {
		if (ftruncate(fd, 0)) {
			fprintf(stderr, "Failed to truncate %s: %s\n", filename, strerror(errno));
			goto fo_close;
		}
		if (json_object_to_fd(fd, root, JSON_C_TO_STRING_PLAIN) < 0) {
			fprintf(stderr, "Failed to write JSON to %s: %s\n", filename, json_util_get_last_err());
			goto fo_close;
		}
	}

fo_close:
	fclose(fo);

	pclose(fp);

	return root;
}

struct json_object * make_safe(const char * name) {
	char filename[40];
	sprintf(filename, "/tmp/olsrd-respondd-%s.json", name);

	struct stat filestat;
	if (stat(filename, &filestat)) {
		// no file, do sync
		return run_safe(filename, name, true);
	}

	double diff = difftime(time(NULL), filestat.st_ctime);
	if (diff > 60 * 10) { // if older than 10 minutes, ignore
		return run_safe(filename, name, true);
	}

	// use cached, update in background
	if (fork()) {
		run_safe(filename, name, false);
		exit(EXIT_SUCCESS);
	}

	json_object * ret = json_object_from_file(filename);
	if (!ret) { // something is messed up
		return run_safe(filename, name, true);
	}

	return ret;
}

__attribute__ ((visibility ("default")))
const struct respondd_provider_info respondd_providers[] = {
	{"nodeinfo", respondd_provider_nodeinfo},
	{"statistics", respondd_provider_statistics},
	{"neighbours", respondd_provider_neighbours},
	{}
};
