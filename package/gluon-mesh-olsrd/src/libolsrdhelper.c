/*
  Copyright (c) 2021, Maciej Krüger <maciej@xeredo.it>
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

#include "libolsrdhelper.h"
#include "uclient.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>

#include <libubox/blobmsg.h>
#include <libubox/uloop.h>

#define USER_AGENT "olsrdhelper (libuclient)"
#define BASE_URL "http://localhost:9001"
#define BASE_URL_LEN sizeof(BASE_URL)

struct recv_json_ctx {
	long size;
  char *data;
};

/** Receives data from uclient and writes it to memory */
static void recv_json_cb(struct uclient *cl) {
	/* struct recv_json_ctx *ctx = uclient_get_custom(cl);
	char buf[1024];
	int len;

	while (true) {
		len = uclient_read_account(cl, buf, sizeof(buf));
		if (len <= 0)
			return;

		printf(
			"\rDownloading image: % 5zi / %zi KiB",
			uclient_data(cl)->downloaded / 1024,
			uclient_data(cl)->length / 1024
		);
		fflush(stdout);

		if (write(ctx->fd, buf, len) < len) {
			fputs("autoupdater: error: downloading firmware image failed: ", stderr);
			perror(NULL);
			return;
		}
	} */
}

int olsrd_get_nodeinfo(char *path, json_object *out) {
  char url[BASE_URL_LEN + strlen(path)];
	sprintf(url, "%s/%s", url, path);

  struct recv_json_ctx json_ctx = { };
	int err_code = get_url(url, &recv_json_cb, &json_ctx, 0);

  if (err_code) {
    return err_code;
  }

  out = json_tokener_parse(json_ctx.data);

  return 0;
}
