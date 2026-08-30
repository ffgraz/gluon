/* SPDX-FileCopyrightText: 2021-2026 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include "providers.h"

#include <gluon-neighbours.h>
#include <json-c/json.h>
#include <libolsrdhelper.h>

struct json_object * respondd_provider_neighbours() {
	json_object * ret = json_object_new_object();
	if (!ret) {
		return NULL;
	}

	json_object * neighs = olsr_get_merged_neighs();
	if (!neighs) {
		json_object_put(ret);
		return NULL;
	}

	json_object_object_add(ret, "batadv", gluon_neighbours_to_batadv(neighs));
	json_object_put(neighs);

	return ret;
}
