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

struct json_object * real_respondd_provider_neighbours(void) {
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

		json_object * intf = json_object_object_get(out, ifname);

		if (!intf) {
			intf = json_object_new_object();
			json_object_object_add(out, ifname, intf);
		}

		json_object_object_add(intf, mac, neigh);
	}

	json_object_object_add(ret, "batadv", out);

	return ret;
}

make_safe_fnc(respondd_provider_neighbours, real_respondd_provider_neighbours)
