#!/usr/bin/env python3
"""Test rig: run mesh test scenarios against a Gluon x86-64 image.

Scenarios run in their own process (pynet keeps global state) and, by
default, two at a time; each gets a pynet slot so the host ports do not
collide and its own directory under tests/run/<scenario>/ holding the
node logs and its output. A scenario limited to specific routing
protocols declares them in a '# protocols: <name>...' comment line;
without it, it runs for every protocol. Pass --proto to say which
protocol the image was built with, so incompatible scenarios are
skipped.

    ./run.py --image ../output/.../gluon-*-x86-64.img.gz --proto babel -j 2
"""

import argparse
import concurrent.futures
import glob
import gzip
import os
import queue
import re
import shutil
import subprocess
import sys

BASE = os.path.dirname(os.path.abspath(__file__))


def prepare_image(path):
    """Accept a .img or a .img.gz; gzipped images are unpacked next to
    the archive and reused while they stay newer than it, so a cached
    image does not have to be unpacked for every run."""
    if not path.endswith('.gz'):
        return path

    unpacked = path[:-3]
    if (not os.path.exists(unpacked)
            or os.path.getmtime(unpacked) < os.path.getmtime(path)):
        print('unpacking %s' % path, flush=True)
        with gzip.open(path, 'rb') as src, open(unpacked, 'wb') as dst:
            shutil.copyfileobj(src, dst)
    return unpacked


def scenario_protocols(path):
    with open(path) as f:
        for line in f:
            m = re.match(r'#\s*protocols:\s*(.+)', line)
            if m:
                return m.group(1).split()
    return None  # all


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--image', default=os.environ.get('GLUON_IMAGE', 'image.img'),
                        help='firmware image, .img or .img.gz '
                             '(default: $GLUON_IMAGE or ./image.img)')
    parser.add_argument('--proto', choices=['batman-adv', 'babel', 'olsrd'],
                        help='routing protocol the image was built with')
    parser.add_argument('-j', '--jobs', type=int, default=2,
                        help='scenarios to run concurrently (default: 2, max 8)')
    parser.add_argument('scenarios', nargs='*',
                        help='scenario names or paths (default: all applicable)')
    args = parser.parse_args()

    if not 1 <= args.jobs <= 8:
        parser.error('--jobs must be 1..8')

    if args.scenarios:
        paths = []
        for s in args.scenarios:
            for candidate in (s, os.path.join(BASE, 'scenarios', s + '.py'), os.path.join(BASE, s)):
                if os.path.isfile(candidate):
                    paths.append(os.path.abspath(candidate))
                    break
            else:
                parser.error('no such scenario: ' + s)
    else:
        paths = sorted(glob.glob(os.path.join(BASE, 'scenarios', '*.py')) +
                       glob.glob(os.path.join(BASE, 'test_*.py')))

    env = dict(os.environ,
               GLUON_IMAGE=os.path.abspath(prepare_image(args.image)),
               PYTHONPATH=BASE + os.pathsep + os.environ.get('PYTHONPATH', ''))

    todo = []
    for path in paths:
        name = os.path.splitext(os.path.basename(path))[0]
        protocols = scenario_protocols(path)
        if args.proto and protocols and args.proto not in protocols:
            print('~~~ %s: skipped (needs %s)' % (name, ' '.join(protocols)), flush=True)
            continue
        todo.append((name, path))

    # Each concurrent scenario gets a slot, which shifts the host ports
    # pynet binds, and its own directory, which keeps images, node logs
    # and ssh keys apart.
    slots = queue.Queue()
    for slot in range(args.jobs):
        slots.put(slot)

    def run_scenario(name, path):
        slot = slots.get()
        try:
            workdir = os.path.join(BASE, 'run', name)
            shutil.rmtree(workdir, ignore_errors=True)
            os.makedirs(workdir)
            logfile = os.path.join(workdir, 'scenario.log')
            with open(logfile, 'wb') as log:
                rc = subprocess.run([sys.executable, '-u', path], cwd=workdir,
                                    env=dict(env, PYNET_SLOT=str(slot)),
                                    stdout=log, stderr=subprocess.STDOUT).returncode
            return name, rc, logfile
        finally:
            slots.put(slot)

    print('running %d scenarios, %d at a time' % (len(todo), args.jobs), flush=True)
    failed = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_scenario, name, path) for name, path in todo]
        for future in concurrent.futures.as_completed(futures):
            name, rc, logfile = future.result()
            if rc != 0:
                print('!!! %s: FAILED (%s)' % (name, logfile), flush=True)
                failed.append(name)
            else:
                print('=== %s: ok' % name, flush=True)

    if failed:
        print('failed scenarios: ' + ' '.join(failed))
        sys.exit(1)


if __name__ == '__main__':
    main()
