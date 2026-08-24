#!/bin/sh

shopt -s nullglob

eval "export FF_${FF_CHANNEL}=true"

ENV=$(env | grep "^FF_")
SELF=$(dirname $(readlink -f "$0"))

(for e in $ENV; do
  echo "export $e"
done

echo "cd $SELF/../.."

cat "$SELF/ci.sh") | lxc exec gluon -- su "$(id -un)" -s /bin/bash -c "bash -"

KEYS=(openwrt/key-build*)
if [ "${#KEYS[*]}" -lt 1 ]; then
  cp -v /storage/ffgraz/key* .
fi

if [ -v FF_experimental ] || [ -v FF_upstream ] || [ -v FF_dev ] || [ -v FF_vanilla_experimental ] || [ -v FF_master ]; then
  contrib/sign.sh /storage/ffgraz/nightly-key output/images/sysupgrade/${FF_CHANNEL}.manifest
fi

rm -rf /storage/ffgraz/www/$FF_CHANNEL
mv output /storage/ffgraz/www/$FF_CHANNEL
