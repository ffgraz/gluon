#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>

#include <libubox/uclient.h>
#include <libubox/blobmsg.h>
#include <libubox/uloop.h>
#include <libolsrdhelper.h>

int main(void) {
	struct json_object *obj;

	printf("Content-type: text/event-stream\n\n");
	fflush(stdout);

	while (1) {
		obj = get_merged_neighs();
		if (obj) {
			printf("data: %s\n\n", json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));
			fflush(stdout);
			json_object_put(obj);
		}
		sleep(10);
	}

	return 0;
}
