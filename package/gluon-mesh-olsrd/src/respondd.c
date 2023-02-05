/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include "respondd-common.h"

#include <respondd.h>

#include <json-c/json.h>

#include <stdio.h>
#include <stdlib.h>

struct json_object * make_safe(const char * name) {
	FILE *fp;

	char exec[40];
	sprintf(exec, "%s %s", "/usr/bin/olsr-respondd", name);

	fp = popen(exec, "r");
	if (fp == NULL) {
		return NULL;
	}

	json_object * root = json_object_from_fd(fileno(fp));

	pclose(fp);

	return root;
}

__attribute__ ((visibility ("default")))
const struct respondd_provider_info respondd_providers[] = {
	{"nodeinfo", respondd_provider_nodeinfo},
	{"neighbours", respondd_provider_neighbours},
	{}
};
