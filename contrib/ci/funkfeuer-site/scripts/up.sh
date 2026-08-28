#!/bin/bash

cd "$(dirname "$0")/../.." || exit 1

git -C site branch -D master-updates || true
bash site/scripts/update-modules.sh
git -C site checkout master
git -C site merge master-updates
git -C site push
