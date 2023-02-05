/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include <libolsrdhelper.h>
#include "respondd-common.h"
#include <stdio.h>

void d(const char * fnc, json_object * res) {
	if (!res) {
		printf("%s: failed\n\n", fnc);
	} else {
		printf("%s: %s\n\n", fnc, json_object_to_json_string_ext(res, JSON_C_TO_STRING_PLAIN));
	}
}

void d2(const char * fnc, json_object * res, int code) {
	if (code) {
		printf("%s: failed %i\n\n", fnc, code);
	} else {
		printf("%s: %s\n\n", fnc, json_object_to_json_string_ext(res, JSON_C_TO_STRING_PLAIN));
	}
}

int main (int argc, char *argv[]) {
	json_object *resp;

	d("provider_neighbours", real_respondd_provider_neighbours());

	d("provider_nodeinfo", real_respondd_provider_nodeinfo());

	d2("olsr1_nodeinfo", resp, olsr1_get_nodeinfo("all", &resp));

	d2("olsr2_nodeinfo", resp, olsr2_get_nodeinfo("nhdpinfo jsonraw link", &resp));

	d("make_safe", make_safe("n"));

	return 0;
}
