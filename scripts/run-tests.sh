#!/usr/bin/env bash
# Run the integration tests against a built image.
# See tests/README.md.

set -e

GLUON_OUTPUTDIR="${GLUON_OUTPUTDIR:-output}"
GLUON_IMAGEDIR="${GLUON_IMAGEDIR:-$GLUON_OUTPUTDIR/images}"
GLUON_TEST_TARGET="${GLUON_TEST_TARGET:-x86-64}"

# OpenWrt builds a host python as a host package, next to the host lua
# gluon already uses; prefer it so the tests run against a known
# interpreter, and install the python dependencies into it.
HOSTPKG_PYTHON='openwrt/staging_dir/hostpkg/bin/python3'
if [ -x "$HOSTPKG_PYTHON" ]; then
	PYTHON="$(pwd)/$HOSTPKG_PYTHON"
else
	PYTHON="$(command -v python3 || true)"
	[ -n "$PYTHON" ] || { echo "no python3 found" >&2; exit 1; }
	echo "note: openwrt host python not built, falling back to $PYTHON" >&2
fi

if ! command -v qemu-system-x86_64 >/dev/null; then
	echo "qemu-system-x86_64 is required (the qemu in the package feed" >&2
	echo "builds qemu for routers, not for the build host)" >&2
	exit 1
fi

if ! "$PYTHON" -c 'import asyncssh' >/dev/null 2>&1; then
	if "$PYTHON" -m pip --version >/dev/null 2>&1; then
		echo "installing python test dependencies into $PYTHON"
		"$PYTHON" -m pip install --quiet -r tests/requirements.txt
	else
		echo "missing python dependencies and no pip in $PYTHON;" >&2
		echo "install them yourself: $(tr '\n' ' ' < tests/requirements.txt)" >&2
		exit 1
	fi
fi

if [ "$(id -u)" != 0 ]; then
	echo "note: not running as root, tests that attach clients will fail" >&2
fi

image=
for candidate in "$GLUON_IMAGEDIR"/factory/*"$GLUON_TEST_TARGET"*.img.gz \
                 "$GLUON_IMAGEDIR"/factory/*"$GLUON_TEST_TARGET"*.img; do
	[ -e "$candidate" ] && { image="$candidate"; break; }
done

if [ -z "$image" ]; then
	echo "no $GLUON_TEST_TARGET image in $GLUON_IMAGEDIR/factory;" >&2
	echo "build one first, e.g. make GLUON_TARGET=$GLUON_TEST_TARGET" >&2
	exit 1
fi

echo "testing $image"
exec "$PYTHON" tests/run.py --image "$image" "$@"
