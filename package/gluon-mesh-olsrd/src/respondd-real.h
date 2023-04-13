/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

struct json_object * real_respondd_provider_neighbours();
struct json_object * real_respondd_provider_nodeinfo();
struct json_object * real_respondd_provider_statistics();
struct json_object * get_traffic(void);
