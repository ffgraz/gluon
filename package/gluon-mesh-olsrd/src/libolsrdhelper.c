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
	int error;
	json_object *parsed;
};

/** Receives data from uclient and writes it to memory */
static void recv_json_cb(struct uclient *cl) {
	struct recv_json_ctx *ctx = uclient_get_custom(cl);

	ctx->size = uclient_data(cl)->length;

	char buf[ctx->size + 1];

	int len = uclient_read_account(cl, buf, sizeof(buf));
	if (len != ctx->size) {
		ctx->error = UCLIENT_ERROR_SIZE_MISMATCH;
		return;
	}

	if (buf[0] != "{") {
		ctx->error = UCLIENT_ERROR_NOT_JSON;
		return;
	}

	buf[ctx->size] = '\0';

	// TODO: handle parser error, add error code for malformed json
	ctx->parsed = json_tokener_parse(&buf);
}

// get enabled from site.conf mesh.olsrX.enabled, get running from service X status
int oi(struct olsr_info **out) {
	struct olsr_info info = {
		.olsr1 = {
			.enabled = false,
			.running = false,
		},
		.olsr2 = {
			.enabled = false,
			.running = false,
		}
	};

	*out = &info;

	return 0;
}

int olsr1_get_nodeinfo(char *path, json_object **out) {
  char url[BASE_URL_LEN + strlen(path) + 2];
	sprintf(url, "%s/%s", url, path);

  struct recv_json_ctx json_ctx = { };
	int err_code = get_url(url, &recv_json_cb, &json_ctx, 0);

  if (err_code) {
    return err_code;
  }

	if (json_ctx.error) {
		return json_ctx.error;
	}

	*out = json_ctx.parsed;

  return 0;
}
