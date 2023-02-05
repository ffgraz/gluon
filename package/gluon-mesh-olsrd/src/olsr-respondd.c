/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include "respondd-common.h"
#include <json-c/json.h>
#include <stdio.h>

int run (json_object * resp) {
	if (!resp) return 1;

	printf("%s", json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN));

	return 0;
}

int main (int argc, char *argv[]) {
	json_object *resp;

	if (argc < 2) {
		goto help;
	}

	switch(argv[1][0]) {
		case 110: { // n
			return run(real_respondd_provider_neighbours());
		}
		case 105: { // i
			return run(real_respondd_provider_nodeinfo());
		}
		default: {
			goto help;
		}
	}

help:
	printf("i = nodeinfo, n = neighbours");
	return 2;
}
