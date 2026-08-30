/* SPDX-FileCopyrightText: 2021-2026 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include "gluon-neighbours.h"

#include <libgluonutil.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Whether source is already listed in the entry's protocols array */
static bool lists_source(json_object *protocols, const char *source) {
	size_t len = json_object_array_length(protocols);

	for (size_t i = 0; i < len; i++) {
		const char *seen = json_object_get_string(json_object_array_get_idx(protocols, i));

		if (seen && !strcmp(seen, source))
			return true;
	}

	return false;
}

static void note_source(json_object *neighbour, const char *source) {
	json_object *protocols = json_object_object_get(neighbour, "protocols");

	if (!protocols) {
		protocols = json_object_new_array();
		if (!protocols)
			return;

		json_object_object_add(neighbour, "protocols", protocols);
	}

	if (!lists_source(protocols, source))
		json_object_array_add(protocols, json_object_new_string(source));
}

void gluon_neighbours_merge(json_object *out, json_object *neighs, const char *source) {
	if (!out || !neighs)
		return;

	json_object_object_foreach(neighs, mac, incoming) {
		json_object *neighbour = json_object_object_get(out, mac);

		if (!neighbour) {
			neighbour = json_object_new_object();
			if (!neighbour)
				return;

			json_object_object_add(out, mac, neighbour);
		}

		note_source(neighbour, source);

		json_object_object_foreach(incoming, key, new) {
			json_object *cur = json_object_object_get(neighbour, key);

			if (!strcmp(key, "tq") || !strcmp(key, "etx")) {
				/* Both daemons rate the same link, so neither
				   number is more right than the other. */
				if (cur) {
					json_object_object_add(
						neighbour,
						key,
						json_object_new_double(
							(json_object_get_double(cur) + json_object_get_double(new)) / 2
						)
					);
				} else {
					json_object_object_add(neighbour, key, json_object_get(new));
				}
			} else if (!strcmp(key, "ip")) {
				/* One address per daemon, and they differ - an
				   olsr4 neighbour is an IPv4 address where a
				   babel one is link-local. */
				char ip_key[64];
				snprintf(ip_key, sizeof(ip_key), "%s_%s", source, key);

				json_object_object_add(neighbour, ip_key, json_object_get(new));
			} else if (!strcmp(key, "best")) {
				if (cur) {
					json_object_object_add(
						neighbour,
						"best",
						json_object_new_boolean(
							json_object_get_boolean(cur) || json_object_get_boolean(new)
						)
					);
				} else {
					json_object_object_add(neighbour, "best", json_object_get(new));
				}
			} else {
				json_object_object_add(neighbour, key, json_object_get(new));
			}
		}
	}
}

json_object * gluon_neighbours_to_batadv(json_object *table) {
	json_object *out = json_object_new_object();

	if (!out || !table)
		return out;

	json_object_object_foreach(table, mac, neighbour) {
		const char *ifname = json_object_get_string(json_object_object_get(neighbour, "ifname"));

		if (!ifname)
			continue;

		char *ifaddr = gluonutil_get_interface_address(ifname);
		if (!ifaddr)
			continue;

		json_object *intf = json_object_object_get(out, ifaddr);
		if (!intf) {
			intf = json_object_new_object();
			json_object_object_add(out, ifaddr, intf);
		}

		free(ifaddr);

		json_object *neighbours = json_object_object_get(intf, "neighbours");
		if (!neighbours) {
			neighbours = json_object_new_object();
			json_object_object_add(intf, "neighbours", neighbours);
		}

		json_object_object_add(neighbours, mac, json_object_get(neighbour));
	}

	return out;
}

bool gluon_neighbours_mac_from_lladdr(const char *lladdr, char mac[18]) {
	struct in6_addr addr;

	if (!lladdr || inet_pton(AF_INET6, lladdr, &addr) != 1)
		return false;

	if (!IN6_IS_ADDR_LINKLOCAL(&addr))
		return false;

	const uint8_t *b = addr.s6_addr;

	/* EUI-64 pads the MAC with ff:fe in the middle and flips the
	   universal/local bit; without that padding the address was not
	   made from a MAC and there is nothing to recover. */
	if (b[11] != 0xff || b[12] != 0xfe)
		return false;

	snprintf(mac, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
		b[8] ^ 0x02, b[9], b[10], b[13], b[14], b[15]);

	return true;
}
