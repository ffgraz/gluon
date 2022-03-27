/*

Copyright 2022 Maciej Krüger <maciej@xeredo.it>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <libolsrdhelper.h>

#define OLSRD "gluon.olsrd"

static int find_module_version (lua_State *L) {
  const char *mod = luaL_checkstring(L, 1);

  DIR *d;
  d = opendir("/usr/lib");

  if (d == NULL)
    return luaL_error(L, "cannot open /usr/lib: %s", strerror(errno));

  struct dirent *entry;
  while ((entry = readdir(d)) != NULL) {
    if (entry->d_type == DT_REG && !strncmp(mod, entry->d_name, strlen(mod))) {
      lua_pushstring(L, entry->d_name);
      closedir(d);
      return 1;
    }
  }

  closedir(d);
  return luaL_error(L, "mod %s not found", mod);
}

static int olsr_nodeinfo (lua_State *L) {
  const char *nodeinfo = luaL_checkstring(L, 1);

  return 0;
}

static const luaL_reg olsrd_methods[] = {
  { "find_module_version", find_module_version },
	{ "olsr_nodeinfo", olsr_nodeinfo },
	{ }
};

int luaopen_gluon_olsrd(lua_State *L)
{
	luaL_register(L, OLSRD, olsrd_methods);

	return 1;
}
