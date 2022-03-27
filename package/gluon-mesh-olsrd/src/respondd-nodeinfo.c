/*
  Copyright (c) 2022, Maciej Krüger <maciej@xeredo.it>
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "respondd-common.h"

#include <libgluonutil.h>

#include <json-c/json.h>

#include <libolsrdhelper.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct json_object * olsr1_get_plugins(void) {
	json_object *resp;
  int error = olsr1_get_nodeinfo("plugins", &resp);

  if (error) {
    return NULL;
  }

	return json_object_object_get(resp, "plugins");
}

static json_object * olsr1_get_version (void) {
	json_object *resp;
  int error = olsr1_get_nodeinfo("version", &resp);

  if (error) {
    return NULL;
  }

	return json_object_object_get(json_object_object_get(resp, "version"), "version");
}

static json_object * olsr2_get_version (void) {
	return NULL;
}

/* static struct json_object * get_mesh(void) {
	struct json_object *ret = json_object_new_object();
	struct json_object *bat0_interfaces = json_object_new_object();
	json_object_object_add(bat0_interfaces, "interfaces", get_mesh_subifs("bat0"));
	json_object_object_add(ret, "bat0", bat0_interfaces);
	return ret;
} */

struct json_object * respondd_provider_nodeinfo(void) {
	struct olsr_info *info;

	struct json_object *ret = json_object_new_object();

	if (oi(&info))
		return ret;

	/* struct json_object *network = json_object_new_object();
	json_object_object_add(network, "addresses", get_addresses());
	json_object_object_add(network, "mesh", get_mesh());
	json_object_object_add(ret, "network", network); */

	/*

	TODO: get interfaces and return in following schema

	{
		interfaces: {
			$intf_name: {
				olsr1: {
					configured,
					up: intf.olsrInterface.up,
				}
				olsr2: {

				}
			}
		}
	}

	*/

	struct json_object *software = json_object_new_object();

	if (info->olsr1.enabled) {
		struct json_object *software_olsr1 = json_object_new_object();

		if (info->olsr1.running) {
			struct json_object *version = olsr1_get_version();
			if (version)
				json_object_object_add(software_olsr1, "version", version);

			struct json_object *plugins = olsr1_get_plugins();
			if (plugins)
				json_object_object_add(software_olsr1, "plugins", plugins);
		}

		json_object_object_add(software, "olsr1", software_olsr1);
	}

	if (info->olsr2.enabled) {
		struct json_object *software_olsr2 = json_object_new_object();

		if (info->olsr2.running) {
			struct json_object *version = olsr2_get_version();
			if (version)
				json_object_object_add(software_olsr2, "version", version);
		}

		json_object_object_add(software, "olsr2", software_olsr2);
	}

	json_object_object_add(ret, "software", software);

	return ret;
}
