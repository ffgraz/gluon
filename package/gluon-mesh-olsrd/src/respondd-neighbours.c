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

static struct json_object * olsr1_get_neigh(void) {
	json_object *resp;
  int error = olsr1_get_nodeinfo("links", &resp);

  if (error) {
    return NULL;
  }

  json_object *out = json_object_new_object();

  if (!out)
    return NULL;

  /*

  links

  localIP	"10.12.11.43"
  remoteIP	"10.12.11.1"
  olsrInterface	"mesh-vpn"
  ifName	"mesh-vpn"
  validityTime	141239
  symmetryTime	123095
  asymmetryTime	25552910
  vtime	124000
  currentLinkStatus	"SYMMETRIC"
  previousLinkStatus	"SYMMETRIC"
  hysteresis	0
  pending	false
  lostLinkTime	0
  helloTime	0
  lastHelloTime	0
  seqnoValid	false
  seqno	0
  lossHelloInterval	3000
  lossTime	3595
  lossMultiplier	65536
  linkCost	1.084961
  linkQuality	1
  neighborLinkQuality	0.921

  */

	json_object *links = json_object_object_get(resp, "links");
	if (!links)
		return NULL;

	int linkcount = json_object_array_length(links);

	for (int i = 0; i < linkcount; i++) {
		struct json_object *link = json_object_array_get_idx(links, i);
		if (!link)
			return NULL;

    struct json_object *neigh = json_object_new_object();
    if (!neigh)
      return NULL;

    json_object_object_add(neigh, "ifname", json_object_object_get(link, "ifName"));
		// TODO: do we need this? should we set this? (we could pick the one peer that we currently route 0.0.0.0 over...)
		json_object_object_add(neigh, "best", json_object_new_boolean(0));
		json_object_object_add(neigh, "etx", json_object_object_get(link, "etx"));

    json_object_object_add(out, json_object_get_string(json_object_object_get(link, "remoteIP")), neigh);
  }

  return out;
}

static struct json_object * olsr2_get_neigh(void) {
	return NULL;
}

struct json_object * respondd_provider_neighbours(void) {
	struct olsr_info *info;

	struct json_object *ret = json_object_new_object();

	if (oi(&info))
		return ret;

	struct json_object *neigh = json_object_new_array();

	if (info->olsr1.enabled && info->olsr1.running) {
		struct json_object *neigh1 = olsr1_get_neigh();
		if (neigh1)
			for (int i = 0; i < json_object_array_length(neigh1); i++)
				json_object_array_add(neigh, json_object_array_get_idx(neigh1, i));
	}

	if (info->olsr2.enabled && info->olsr2.running) {
		struct json_object *neigh2 = olsr2_get_neigh();
		if (neigh2)
			for (int i = 0; i < json_object_array_length(neigh2); i++)
				json_object_array_add(neigh, json_object_array_get_idx(neigh2, i));
	}

	json_object_object_add(ret, "neigh", neigh);

	return ret;
}
