/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#include <json-c/json.h>
#include <libubox/uclient.h>
#include <libgluonutil.h>
#include <uci.h>

struct olsr1_info {
	bool enabled;
	bool running;
};

struct olsr2_info {
	bool enabled;
	bool running;
};

struct olsr_info {
	struct olsr1_info olsr1;
	struct olsr2_info olsr2;
};

int oi(struct olsr_info **out);

int olsr1_get_nodeinfo(const char *path, json_object **out);

int olsr2_get_nodeinfo(const char *cmd, json_object **out);
int olsr2_get_nodeinfo_raw(const char *cmd, char **out);

struct json_object * olsr1_get_neigh(void);
struct json_object * olsr2_get_neigh(void);
void merge_neighs(json_object * out, json_object * neighs, char * version);

// stuff that could be in a shared library named responddhelper

/*
	out is an optional parameter. If not set the raw fd will be returned.
	Example:
		int fd = socket_request("/var/run/mmfd.sock", "get_neighbours", NULL);
		if (fd < 0) return NULL;
		struct json_object * response = json_object_from_fd(fd);
*/
int socket_request(const char *path, const char *cmd, char **out);

json_object * socket_request_json(const char *path, const char *cmd);