/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#define make_safe_fnc(name, arg) \
struct json_object * name (void) { \
	return make_safe(&(arg)); \
}

struct json_object * make_safe(const char * name);

struct json_object * respondd_provider_nodeinfo();
struct json_object * respondd_provider_neighbours();
struct json_object * real_respondd_provider_nodeinfo();
struct json_object * real_respondd_provider_neighbours();
