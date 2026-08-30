/* SPDX-FileCopyrightText: 2026 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#include <json-c/json.h>

#include <stdbool.h>

/*
	A layer-3 node runs more than one routing daemon at a time - olsrd for
	IPv4 next to babel for IPv6, and olsrd is itself two daemons - and each
	of them sees the same physical neighbour over the same interface.
	respondd answers with one neighbour table, so their views have to be
	merged into it instead of overwriting each other.

	Everything here works on a table keyed by the neighbour's MAC, the one
	identifier every daemon can be reduced to.
*/

/**
 * Merges one source's neighbours into out, both keyed by neighbour MAC.
 *
 * source names the daemon the entries came from ("olsr4", "babel", ...).
 * Metrics that several sources report (tq, etx) are averaged, addresses
 * are kept apart as <source>_ip so they do not overwrite one another,
 * best is true as soon as any source says so, and the sources that saw a
 * neighbour are listed in protocols. Anything else is taken from the
 * source that reported it last.
 */
void gluon_neighbours_merge(json_object *out, json_object *neighs, const char *source);

/**
 * Renders a MAC-keyed table as the batman-adv shaped block respondd
 * reports, so consumers see a layer-3 mesh the way they see a
 * batman-adv one:
 *
 *   { "<local interface MAC>": { "neighbours": { "<neighbour MAC>": { ... } } } }
 *
 * Entries need an "ifname" saying where the neighbour was seen; ones
 * without it, or on an interface that has no MAC, are skipped. Returns a
 * new object the caller owns, or NULL when out of memory.
 */
json_object * gluon_neighbours_to_batadv(json_object *table);

/**
 * Recovers a neighbour's MAC from its EUI-64 link-local address, for the
 * daemons that only ever name a neighbour by address. Writes
 * "aa:bb:cc:dd:ee:ff" to mac. False when the address is not link-local,
 * or not derived from a MAC in the first place.
 */
bool gluon_neighbours_mac_from_lladdr(const char *lladdr, char mac[18]);
