/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include "respondd-common.h"

#include <libgluonutil.h>

#include <json-c/json.h>

#include <libolsrdhelper.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct json_object * real_respondd_provider_neighbours() {
	struct olsr_info *info;

	struct json_object *ret = json_object_new_object();
	struct json_object *out = json_object_new_object();

	if (oi(&info))
		return ret;

	struct json_object *neighs = json_object_new_object();

	if (info->olsr1.enabled && info->olsr1.running) {
		struct json_object *neigh1 = olsr1_get_neigh();
		if (neigh1)
			merge_neighs(neighs, neigh1, "olsr1");
	}

	if (info->olsr2.enabled && info->olsr2.running) {
		struct json_object *neigh2 = olsr2_get_neigh();
		if (neigh2)
			merge_neighs(neighs, neigh2, "olsr2");
	}

	json_object_object_foreach(neighs, mac, neigh) {
		const char * ifname = json_object_get_string(json_object_object_get(neigh, "ifname"));

		if (!ifname) {
			continue;
		}

		char *ifaddr = gluonutil_get_interface_address(ifname);
		if (!ifaddr) {
			continue;
		}

		json_object * intf = json_object_object_get(out, ifaddr);
		if (!intf) {
			intf = json_object_new_object();
			json_object_object_add(out, ifaddr, intf);
		}

		json_object * ifneigh = json_object_object_get(intf, "neighbours");
		if (!ifneigh) {
			ifneigh = json_object_new_object();
			json_object_object_add(intf, "neighbours", ifneigh);
		}

		json_object_object_add(ifneigh, mac, neigh);
	}

	json_object_object_add(ret, "batadv", out);

	return ret;
}

make_safe_fnc(respondd_provider_neighbours, "n")
