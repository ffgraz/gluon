/* SPDX-FileCopyrightText: 2023, Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include "respondd-common.h"

#include <libolsrdhelper.h>

#include <json-c/json.h>

#include <stdlib.h>
#include <string.h>

#include <net/if.h>
#include <linux/if_link.h>

#include <linux/rtnetlink.h>

static void add_gateway(struct json_object *obj) {
	// TODO: get gateway (like in state-check scripts)
	// note: we can only specify ONE gateway so we'll have trouble with
	// ipv4+ipv6 having different gateways
	// { gateway: mac, gateway_tq: tq, gateway_nexthop: best_node_mac }
}

int rtnl_get_link(struct rtnl_link_stats64 ** out) {
    *out = NULL;

    int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0) {
        return errno;
    }

    struct sockaddr_nl recv_addr;
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.nl_family = AF_NETLINK;
    recv_addr.nl_pid = getpid();
    recv_addr.nl_groups = 0;

    if (bind(sock, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0) {
        goto socket_end;
    }

    struct sockaddr_nl dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;
    dest_addr.nl_groups = 0;

    struct {
        struct nlmsghdr nh;
        struct ifinfomsg ifmsg;
        char attrbuf[512];
    } req;
    struct rtattr *rta;

    memset(&req, 0, sizeof(req));
    req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nh.nlmsg_flags = NLM_F_REQUEST;
    req.nh.nlmsg_type = RTM_GETLINK;
    req.ifmsg.ifi_family = AF_UNSPEC;
    req.ifmsg.ifi_index = if_nametoindex("local-port");
    req.ifmsg.ifi_change = 0xffffffff; /* ??? */

    rta = (struct rtattr *)(((char *) &req) +
                            NLMSG_ALIGN(req.nh.nlmsg_len));
    rta->rta_type = IFLA_STATS;
    rta->rta_len = RTA_LENGTH(sizeof(unsigned int));
    req.nh.nlmsg_len = NLMSG_ALIGN(req.nh.nlmsg_len) +
                       RTA_LENGTH(sizeof(struct rtnl_link_stats));

    if (send(sock, &req, req.nh.nlmsg_len, 0) < 0) {
        return errno;
    }

    char buf[4096];
    if (recv(sock, &buf, 4096, 0) < 0)
        return errno;

    struct nlmsghdr *recv_hdr = (struct nlmsghdr*)buf;
    struct ifinfomsg *infomsg = NLMSG_DATA(recv_hdr);

    rta = IFLA_RTA(infomsg);
    int len = recv_hdr->nlmsg_len;

    while (RTA_OK(rta, len)){
        if (rta->rta_type == IFLA_STATS64) {
            struct rtnl_link_stats64 *stats64 = RTA_DATA(rta);
            struct rtnl_link_stats64 *copy = malloc(sizeof(struct rtnl_link_stats64));
            if (!copy) {
                errno = ENOMEM;
                goto socket_end;
            }
            memcpy(copy, stats64, sizeof(struct rtnl_link_stats64));
            *out = copy;
            break;
        }

        rta = RTA_NEXT(rta, len);
    }

    if (!*out) {
        return ENODATA;
    }

socket_end:
    close(sock);
end:
    return errno;
}

#define SET_STAT(jsonc, field) \
	json_object_object_add((jsonc), #field, \
		json_object_new_int64(rtnl->jsonc ## _ ## field));

struct json_object * get_traffic(void) {
	struct json_object *out = NULL;

	struct rtnl_link_stats64 * rtnl;

	if (rtnl_get_link(&rtnl))
		goto end;

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

	out = json_object_new_object();
	json_object_object_add(out, "rx", rx);
	json_object_object_add(out, "tx", tx);

	free(rtnl);

end:
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

struct json_object * real_respondd_provider_statistics(void) {
	struct json_object *ret = json_object_new_object();

	json_object_object_add(ret, "clients", get_clients());
	json_object_object_add(ret, "traffic", get_traffic());

	add_gateway(ret);

	return ret;
}

make_safe_fnc(respondd_provider_statistics, "s")
