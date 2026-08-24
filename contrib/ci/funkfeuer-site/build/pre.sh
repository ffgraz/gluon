#!/bin/sh

SELF=$(dirname $(readlink -f "$0"))

if ! lxc info gluon 2>/dev/null >/dev/null; then
  bash "$SELF/lxd.sh"
fi

