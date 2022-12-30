#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>

#include <libubox/uclient.h>
#include <libubox/blobmsg.h>
#include <libubox/uloop.h>
#include <libolsrdhelper.h>

static json_object *neighbours(void) {
	struct olsr_info *info;

	if (oi(&info))
		return NULL;

	json_object *out = json_object_new_object();
	if (!out) {
		return NULL;
	}

	if (info->olsr2.running) {
		json_object *olsr2_neigh;

		olsr2_neigh = olsr2_get_neigh();
		if (!olsr2_neigh) {
			return NULL;
		}

		merge_neighs(out, olsr2_neigh, "olsr2");
	}

	if (info->olsr1.running) {
		json_object *olsr1_neigh;

		olsr1_neigh = olsr1_get_neigh();
		if (!olsr1_neigh) {
			return NULL;
		}

		merge_neighs(out, olsr1_neigh, "olsr1");
	}

	return out;
}

int main(void) {
	struct json_object *obj;

	printf("Content-type: text/event-stream\n\n");
	fflush(stdout);

	while (1) {
		obj = neighbours();
		if (obj) {
			printf("data: %s\n\n", json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));
			fflush(stdout);
			json_object_put(obj);
		}
		sleep(10);
	}

	return 0;
}
