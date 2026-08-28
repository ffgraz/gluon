/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

/*
	olsr-macd remembers which MAC an OLSR neighbour address was last seen
	with and answers on this socket:

		dump                    -> { "<ifname>": { "<ip>": "<mac>" } }
		resolve <ifname> <ip>   -> the MAC, or an empty line
*/
#define OLSR_MACD_SOCKET "/var/run/olsr-macd.sock"
