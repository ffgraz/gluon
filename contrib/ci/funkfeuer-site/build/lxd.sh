#!/bin/sh

lxc launch images:debian/12 -c security.privileged=true gluon
yes "
" | lxc exec gluon -- adduser --uid "$(id -u)" "$(id -un)"
SRC=$(readlink -f ..)
lxc exec gluon apt update
yes | lxc exec gluon apt install build-essential python3 python3-distutils libncurses-dev unzip make git gawk wget python3 python3-distutils rsync qemu-utils
yes | lxc exec gluon apt install ca-certificates file git subversion python3 build-essential gawk unzip libncurses5-dev zlib1g-dev libssl-dev libelf-dev wget rsync time qemu-utils ecdsautils lua-check shellcheck
lxc config device add gluon src disk "source=$SRC" "path=$SRC"
