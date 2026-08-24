/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#include <json-c/json.h>

struct json_object * respondd_provider_neighbours(void);
struct json_object * respondd_provider_nodeinfo(void);
struct json_object * respondd_provider_statistics(void);

struct json_object * get_traffic(void);
