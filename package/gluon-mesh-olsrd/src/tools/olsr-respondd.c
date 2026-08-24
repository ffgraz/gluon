/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include "providers.h"

#include <json-c/json.h>
#include <stdio.h>

static int run (json_object *resp) {
	if (!resp)
		return 1;

	printf("%s", json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN));

	json_object_put(resp);

	return 0;
}

int main (int argc, char *argv[]) {
	if (argc >= 2) {
		switch (argv[1][0]) {
		case 's':
			return run(respondd_provider_statistics());
		case 'n':
			return run(respondd_provider_neighbours());
		case 'i':
			return run(respondd_provider_nodeinfo());
		}
	}

	printf("i = nodeinfo, n = neighbours, s = statistics\n");

	return 2;
}
