/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include "respondd-common.h"

#include <libgluonutil.h>
#include <json-c/json.h>
#include <libolsrdhelper.h>
#include <string.h>

struct json_object * real_respondd_provider_neighbours() {
	json_object * ret = json_object_new_object();
	if (!ret) {
		return NULL;
	}

	json_object * out = json_object_new_object();
	if (!out) {
		goto fail;
	}

	json_object_object_add(ret, "batadv", out);

	json_object * neighs = get_merged_neighs();

	if (!neighs) {
		goto fail;
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

		json_object_object_add(ifneigh, mac, json_object_get(neigh));
	}

	json_object_put(neighs);

end:
	return ret;

fail:
	json_object_put(ret);
	ret = NULL;
	goto end;
}

make_safe_fnc(respondd_provider_neighbours, "n")
