#!/usr/bin/env bash

set -eo pipefail

echo "Consider your warranty void now. Upgrading to openwrt master, temporarly."

# move to basedir, in case the script is not executed via `make update-modules`
cd "$(dirname "$0")" || exit 1

git checkout -- modules

sed "s|openwrt-22.03|master|g" -i modules

# shellcheck source=./modules
source ./modules

LOCAL_BRANCH=$(git branch --show-current)
[[ $LOCAL_BRANCH != *-updates ]] && LOCAL_BRANCH+=-updates

for MODULE in "OPENWRT" "PACKAGES_PACKAGES" "PACKAGES_ROUTING" "PACKAGES_GLUON"; do
	_REMOTE_URL=${MODULE}_REPO
	_REMOTE_BRANCH=${MODULE}_BRANCH
	_LOCAL_HEAD=${MODULE}_COMMIT

	REMOTE_URL="${!_REMOTE_URL}"
	REMOTE_BRANCH="${!_REMOTE_BRANCH}"
	LOCAL_HEAD="${!_LOCAL_HEAD}"

	# get default branch name if none is set
	[ -z "${REMOTE_BRANCH}" ] && {
		REMOTE_BRANCH=$(git ls-remote --symref "${REMOTE_URL}" HEAD | awk '/^ref:/ { sub(/refs\/heads\//, "", $2); print $2 }')
	}

	# fetch the commit id for the HEAD of the module
	REMOTE_HEAD=$(git ls-remote "${REMOTE_URL}" "${REMOTE_BRANCH}" | awk '{ print $1 }')

	# skip ahead if the commit id did not change
	[ "$LOCAL_HEAD" == "$REMOTE_HEAD" ] && continue 1

	echo "$REMOTE_URL $REMOTE_HEAD"

	# modify modules file
	sed -i "s/${LOCAL_HEAD}/${REMOTE_HEAD}/" ./modules
done

