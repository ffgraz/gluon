/* SPDX-FileCopyrightText: 2023, Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include "providers.h"

#include <libolsrdhelper.h>

#include <json-c/json.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/socket.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>

/** Interface all client traffic passes through */
#define TRAFFIC_INTERFACE "local-port"

static void add_gateway(struct json_object *obj) {
	// TODO: get gateway (like in state-check scripts)
	// note: we can only specify ONE gateway so we'll have trouble with
	// ipv4+ipv6 having different gateways
	// { gateway: mac, gateway_tq: tq, gateway_nexthop: best_node_mac }
}

/** Reads the interface counters of TRAFFIC_INTERFACE, returns 0 on success */
static int rtnl_get_link(struct rtnl_link_stats64 **out) {
	*out = NULL;

	unsigned int ifindex = if_nametoindex(TRAFFIC_INTERFACE);
	if (!ifindex)
		return errno;

	int sock = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
	if (sock < 0)
		return errno;

	int err;

	struct {
		struct nlmsghdr nh;
		struct ifinfomsg ifmsg;
	} req = {
		.nh = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
			.nlmsg_type = RTM_GETLINK,
			.nlmsg_flags = NLM_F_REQUEST,
		},
		.ifmsg = {
			.ifi_family = AF_UNSPEC,
			.ifi_index = ifindex,
		},
	};

	if (send(sock, &req, req.nh.nlmsg_len, 0) < 0) {
		err = errno;
		goto end;
	}

	char buf[8192];
	int len = recv(sock, buf, sizeof(buf), 0);

	if (len < 0) {
		err = errno;
		goto end;
	}

	err = ENODATA;

	struct nlmsghdr *nh = (struct nlmsghdr *)buf;
	if (!NLMSG_OK(nh, len) || nh->nlmsg_type != RTM_NEWLINK)
		goto end;

	struct ifinfomsg *infomsg = NLMSG_DATA(nh);
	int rtalen = IFLA_PAYLOAD(nh);

	for (struct rtattr *rta = IFLA_RTA(infomsg); RTA_OK(rta, rtalen); rta = RTA_NEXT(rta, rtalen)) {
		if (rta->rta_type != IFLA_STATS64 || RTA_PAYLOAD(rta) < sizeof(struct rtnl_link_stats64))
			continue;

		struct rtnl_link_stats64 *copy = malloc(sizeof(struct rtnl_link_stats64));
		if (!copy) {
			err = ENOMEM;
			goto end;
		}

		memcpy(copy, RTA_DATA(rta), sizeof(struct rtnl_link_stats64));
		*out = copy;
		err = 0;
		break;
	}

end:
	close(sock);

	return err;
}

#define SET_STAT(jsonc, field) \
	json_object_object_add((jsonc), #field, \
		json_object_new_int64(rtnl->jsonc ## _ ## field));

struct json_object * get_traffic(void) {
	struct rtnl_link_stats64 *rtnl;

	if (rtnl_get_link(&rtnl))
		return NULL;

	struct json_object *rx = json_object_new_object();
	SET_STAT(rx, bytes);
	SET_STAT(rx, packets);
	SET_STAT(rx, dropped);
	SET_STAT(rx, errors);

	struct json_object *tx = json_object_new_object();
	SET_STAT(tx, bytes);
	SET_STAT(tx, packets);
	SET_STAT(tx, dropped);
	SET_STAT(tx, errors);

	free(rtnl);

	struct json_object *out = json_object_new_object();
	json_object_object_add(out, "rx", rx);
	json_object_object_add(out, "tx", tx);

	return out;
}

static struct json_object * get_clients(void) {
	struct json_object * response = socket_request_json("/var/run/l3roamd.sock", "get_clients");
	if (!response) return NULL;

	struct json_object *ret = json_object_new_object();

	J_OCPY2(ret, response, "total", "clients");
	json_object_object_add(ret, "list", json_object_object_get(response, "clientlist") ? J_OGET(response, "clientlist") : json_object_new_object());

	json_object_put(response);

	return ret;
}

struct json_object * respondd_provider_statistics(void) {
	struct json_object *ret = json_object_new_object();

	json_object_object_add(ret, "clients", get_clients());
	json_object_object_add(ret, "traffic", get_traffic());

	add_gateway(ret);

	return ret;
}
