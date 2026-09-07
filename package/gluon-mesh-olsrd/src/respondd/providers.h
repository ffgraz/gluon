/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#include <json-c/json.h>
#include <libgluonutil.h>
#include <libolsrdhelper.h>

/* libolsrdhelper does not load the site config itself, we do */
static inline int olsr_get_site_info(struct olsr_info *out) {
	struct json_object *site = gluonutil_load_site_config();
	int err = olsr_get_info(site, out);
	json_object_put(site);
	return err;
}

struct json_object * respondd_provider_neighbours(void);
struct json_object * respondd_provider_nodeinfo(void);
struct json_object * respondd_provider_statistics(void);

struct json_object * get_traffic(void);
