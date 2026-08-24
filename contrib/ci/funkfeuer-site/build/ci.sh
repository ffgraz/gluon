#!/bin/bash

shopt -s nullglob

if [ -v FF_master ]; then
  bash master-modules.sh
fi

make update

KEYS=(key-build*)
if [ "${#KEYS[*]}" -ge 1 ]; then
  mv -v key-build* openwrt
fi

cores=$(( $(cat /proc/cpuinfo | awk '/^processor/{print $3}' | tail -1) + 1 ))

PARAMS=(GLUON_AUTOUPDATER_BRANCH=$FF_CHANNEL)

if [ -v FF_experimental ] || [ -v FF_upstream ] || [ -v FF_dev ] || [ -v FF_master ] || [ -v FF_vanilla_experimental ]; then
  PARAMS+=(GLUON_AUTOUPDATER_ENABLED=1)
fi

if [ -v FF_vanilla_experimental ] || [ -v FF_vanilla ]; then
  PARAMS+=(GLUON_PREFIX=funkfeuer-graz-vanilla GLUON_BUILDTYPE=ffgraz)
fi

if [ -v FF_RELEASE ]; then
  PARAMS+=(GLUON_RELEASE="$FF_RELEASE")
fi

echo "Cleaning..."

rm -rf output

# for target in $(make list-targets); do
#   make "GLUON_TARGET=$target" clean
# done

for target in $(make list-targets); do
  echo "Building $target..."
  make "GLUON_TARGET=$target" "-j$cores" "${PARAMS[@]}"
done

make manifest "${PARAMS[@]}"
