#!/usr/bin/env python3
"""Test rig: run mesh test scenarios against a Gluon x86-64 image.

Each scenario is executed in its own process (pynet keeps global
state). A scenario limited to specific routing protocols declares them
in a '# protocols: <name>...' comment line; without it, it runs for
every protocol. Pass --proto to say which protocol the image was built
with, so incompatible scenarios are skipped.

    ./run.py --image ../output/.../gluon-*-x86-64.img --proto babel
"""

import argparse
import glob
import os
import re
import subprocess
import sys

BASE = os.path.dirname(os.path.abspath(__file__))


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
                        help='firmware image (default: $GLUON_IMAGE or ./image.img)')
    parser.add_argument('--proto', choices=['batman-adv', 'babel', 'olsrd'],
                        help='routing protocol the image was built with')
    parser.add_argument('scenarios', nargs='*',
                        help='scenario names or paths (default: all applicable)')
    args = parser.parse_args()

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
               GLUON_IMAGE=os.path.abspath(args.image),
               PYTHONPATH=BASE + os.pathsep + os.environ.get('PYTHONPATH', ''))

    failed = []
    for path in paths:
        name = os.path.splitext(os.path.basename(path))[0]
        protocols = scenario_protocols(path)
        if args.proto and protocols and args.proto not in protocols:
            print('~~~ %s: skipped (needs %s)' % (name, ' '.join(protocols)), flush=True)
            continue
        print('=== %s' % name, flush=True)
        if subprocess.run([sys.executable, path], cwd=BASE, env=env).returncode != 0:
            print('!!! %s: FAILED' % name, flush=True)
            failed.append(name)
        else:
            print('=== %s: ok' % name, flush=True)

    if failed:
        print('failed scenarios: ' + ' '.join(failed))
        sys.exit(1)


if __name__ == '__main__':
    main()
