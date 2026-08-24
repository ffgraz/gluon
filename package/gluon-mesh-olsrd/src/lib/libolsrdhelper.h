/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#include <json-c/json.h>
#include <libubox/uclient.h>
#include <libgluonutil.h>

#include <stdbool.h>

/*
	olsrd runs one daemon per address family: olsrd for IPv4 and olsrd6 for
	IPv6. Both speak the same jsoninfo API, each one on its own port, so
	every query takes the address family it is meant for.
*/
#define OLSR_IPV4 4
#define OLSR_IPV6 6

struct olsr_daemon_info {
	bool enabled;
	bool running;
};

struct olsr_info {
	struct olsr_daemon_info olsr4;
	struct olsr_daemon_info olsr6;
};

/** Status of the daemon of the given address family, NULL if unknown */
static inline struct olsr_daemon_info * olsr_daemon(struct olsr_info *info, int ipv) {
	switch (ipv) {
	case OLSR_IPV4:
		return &info->olsr4;
	case OLSR_IPV6:
		return &info->olsr6;
	default:
		return NULL;
	}
}

/** Name of the daemon of the given address family as used in respondd output */
static inline const char * olsr_name(int ipv) {
	return ipv == OLSR_IPV4 ? "olsr4" : "olsr6";
}

int olsr_get_info(struct olsr_info *out);

int olsr_get_nodeinfo(int ipv, const char *path, json_object **out);

struct json_object * olsr_get_neigh(int ipv);
struct json_object * olsr_get_merged_neighs(void);

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

// macros for json c

#define J_OUT(x) json_object *out = json_object_get((x));	\
	json_object_put(resp);					\
	return out;

#define J_OGET(obj, key) json_object_get(json_object_object_get(obj, key))

#define J_OCPY(dst, src, key) json_object_object_add(dst, key, json_object_get(json_object_object_get(src, key)))

#define J_OCPY2(dst, src, dkey, skey) json_object_object_add(dst, dkey, json_object_get(json_object_object_get(src, skey)))
