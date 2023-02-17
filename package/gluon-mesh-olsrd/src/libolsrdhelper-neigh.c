/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include "libolsrdhelper.h"
#include <stdio.h>
#include "arp.h"

void merge_neighs(json_object * out, json_object * neighs, char * version) {
	json_object_object_foreach(neighs, mac, neighbour_original) {
		json_object * neighbour = json_object_object_get(out, mac);

		if (!neighbour) {
			neighbour = json_object_new_object();
			json_object_object_add(out, mac, neighbour);
		}

		json_object_object_foreach(neighbour_original, key, new) {
			json_object * cur = json_object_object_get(neighbour, key);

			if (!strcmp(key, "tq")) {
				if (cur) {
					json_object_object_add(
							neighbour,
							"tq",
							json_object_new_double(
									(json_object_get_double(cur) + json_object_get_double(new)) / 2
							)
					);
				} else {
					json_object_object_add(neighbour, "tq", json_object_get(new));
				}
			} else if (!strcmp(key, "etx")) {
				if (cur) {
					json_object_object_add(
							neighbour,
							"etx",
							json_object_new_double(
									(json_object_get_double(cur) + json_object_get_double(new)) / 2
							)
					);
				} else {
					json_object_object_add(neighbour, "etx", json_object_get(new));
				}
			} else if (!strcmp(key, "ip")) {
				char str[10];
				sprintf(str, "%s_%s", version, key);

				json_object_object_add(neighbour, str, json_object_get(new));
			} else if (!strcmp(key, "best")) {
				if (cur) {
					json_object_object_add(
						neighbour,
						"best",
						json_object_new_boolean(
							json_object_get_boolean(cur) || json_object_get_boolean(new)
						)
					);
				} else {
					json_object_object_add(neighbour, "best", json_object_get(new));
				}
			} else {
				json_object_object_add(neighbour, key, json_object_get(new));
			}
		}
	}
}

json_object * get_merged_neighs() {
	struct olsr_info info;

	if (oi(&info))
		goto end;

	json_object *out = json_object_new_object();
	if (!out) {
		goto end;
	}

	if (info.olsr2.running) {
		json_object *olsr2_neigh;

		olsr2_neigh = olsr2_get_neigh();
		if (!olsr2_neigh) {
			goto fail;
		}

		merge_neighs(out, olsr2_neigh, "olsr2");
		json_object_put(olsr2_neigh);
	}

	if (info.olsr1.running) {
		json_object *olsr1_neigh;

		olsr1_neigh = olsr1_get_neigh();
		if (!olsr1_neigh) {
			goto fail;
		}

		merge_neighs(out, olsr1_neigh, "olsr1");
		json_object_put(olsr1_neigh);
	}

	goto end;

fail:
	json_object_put(out);
	out = NULL;
end:
	return out;
}

struct json_object * olsr1_get_neigh(void) {
	json_object *out = NULL;
	json_object *resp;

	if (olsr1_get_nodeinfo("links", &resp)) {
		goto cleanup;
	}

	// olsr1 does not give us the mac that the other side uses
	// this is bad, since macs are the magic uuids in gluon
	// but since it's IP we can just do ARP

	// note that we run on the assumption that we've already commounicated with this ip,
	// otherwise we just ping it

	struct arp_cache * cache = read_arp_cache();
	if (!cache) {
		goto cleanup;
	}

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
	if (!links) {
		goto cleanup_resp;
	}

	out = json_object_new_object();

	if (!out) {
		goto cleanup_cache;
	}

	size_t linkcount = json_object_array_length(links);

	for (size_t i = 0; i < linkcount; i++) {
		struct json_object *link = json_object_array_get_idx(links, i);
		if (!link) {
			goto fail;
		}

		struct json_object *neigh = json_object_new_object();
		if (!neigh) {
			goto fail;
		}

		J_OCPY2(neigh, link, "ifname", "ifName");
		// set this if we detect peer in hna is doing gw
		json_object_object_add(neigh, "best", json_object_new_boolean(0));

		const double linkQuality = json_object_get_double(json_object_object_get(link, "linkQuality"));
		const double neighborLinkQuality = json_object_get_double(json_object_object_get(link, "neighborLinkQuality"));

		json_object_object_add(neigh, "etx", json_object_new_double(1 / (linkQuality * neighborLinkQuality)));
		J_OCPY2(neigh, link, "ip", "remoteIP");

		json_object_object_add(neigh, "tq", json_object_new_double(255 * (linkQuality * neighborLinkQuality)));

		char * mac = resolve_mac(
			cache,
			json_object_get_string(json_object_object_get(link, "ifName")),
			json_object_get_string(json_object_object_get(link, "remoteIP")),
			true
		);

		if (!mac) {
			continue;
		}

		json_object_object_add(out, mac, neigh);

		free(mac);
	}

	goto cleanup_resp;

fail:
	json_object_put(out);
	out = NULL;
cleanup_resp:
	json_object_put(resp);
cleanup_cache:
	cleanup_arp_cache(cache);
cleanup:
	return out;
}

struct json_object * olsr2_get_neigh(void) {
	json_object *out = NULL;
	json_object *resp;

	if (olsr2_get_nodeinfo("nhdpinfo jsonraw link", &resp)) {
		goto cleanup;
	}

	/*

	links

	"if":"olsr12",
	"link_bindto":"fe80::ec67:efff:fed3:d856",
	"link_vtime_value":20,
	"link_itime_value":2,
	"link_symtime":19.786,
	"link_heardtime":19.886,
	"link_vtime":39.886,
	"link_status":"symmetric",
	"link_dualstack":"-",
	"link_mac":"b2:d6:9d:88:c1:58",
	"link_flood_local":"true",
	"link_flood_remote":"true",
	"link_flood_willingness":7,
	"neighbor_originator":"fdff:182f:da60:abc::66",
	"neighbor_dualstack":"-",
	"domain":0,
	"domain_metric":"ff_dat_metric",
	"domain_metric_in":"1.02kbit/s",
	"domain_metric_in_raw":2105088,
	"domain_metric_out":"1.02kbit/s",
	"domain_metric_out_raw":2105088,

	*/

	json_object *links = json_object_object_get(resp, "link");
	if (!links) {
		goto cleanup_resp;
	}

	size_t linkcount = json_object_array_length(links);

	out = json_object_new_object();

	if (!out) {
		goto cleanup_resp;
	}

	for (size_t i = 0; i < linkcount; i++) {
		struct json_object *link = json_object_array_get_idx(links, i);
		if (!link) {
			goto fail;
		}

		struct json_object *neigh = json_object_new_object();
		if (!neigh) {
			goto fail;
		}

		J_OCPY2(neigh, link, "ifname", "if");
		// set this if nhdpinfo returns this peer as being used for :: or 0.0.0.0
		json_object_object_add(neigh, "best", json_object_new_boolean(0));
		J_OCPY2(neigh, link, "etx", "link_vtime");
		J_OCPY2(neigh, link, "ip", "neighbor_originator");

		json_object_object_add(out, json_object_get_string(json_object_object_get(link, "link_mac")), neigh);
	}

	goto cleanup_resp;

fail:
	json_object_put(out);
	out = NULL;
cleanup_resp:
	json_object_put(resp);
cleanup:
	return out;
}
