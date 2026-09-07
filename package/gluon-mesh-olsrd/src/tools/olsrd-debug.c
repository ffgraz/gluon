/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include <libolsrdhelper.h>
#include "providers.h"

#include <stdio.h>
#include <json-c/json.h>

/** Prints a json object and releases it */
static void d(const char *fnc, json_object *res) {
	if (!res) {
		printf("%s: failed\n\n", fnc);
		return;
	}

	printf("%s: %s\n\n", fnc, json_object_to_json_string_ext(res, JSON_C_TO_STRING_PLAIN));

	json_object_put(res);
}

/** Prints the result of a call that returns an error code */
static void d2(const char *fnc, json_object *res, int code) {
	if (code) {
		printf("%s: failed %i\n\n", fnc, code);
		return;
	}

	d(fnc, res);
}

int main (void) {
	json_object *resp = NULL;

	d("get_traffic", get_traffic());

	d("provider_neighbours", respondd_provider_neighbours());

	d("provider_nodeinfo", respondd_provider_nodeinfo());

	d("provider_statistics", respondd_provider_statistics());

	int err = olsr_get_nodeinfo(OLSR_IPV4, "version", &resp);
	d2("olsr4_nodeinfo", resp, err);

	resp = NULL;
	err = olsr_get_nodeinfo(OLSR_IPV6, "version", &resp);
	d2("olsr6_nodeinfo", resp, err);

	d("l3roamd_clients", socket_request_json("/var/run/l3roamd.sock", "get_clients"));

	return 0;
}
