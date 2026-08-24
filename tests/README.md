# Integration tests

QEMU/KVM-based mesh integration tests, driven by the vendored
[pynet](pynet/README.md) module.

Layout:

- `pynet/` — boots x86-64 images in QEMU and drives them over SSH
- `meshlib.py` — topology helpers (`pair()`, `chain(n)`) and
  protocol abstractions (`wait_neighbours()`, `ping()`, `node_addr()`)
  that auto-detect batman-adv/babel/olsrd on the running image
- `scenarios/` — protocol-independent test scenarios built on meshlib
- `test_*.py` — standalone scenarios; a `# protocols: <name>...`
  header restricts them to matching images
- `run.py` — runs a scenario set against one image

Usage:

```sh
pip install -r requirements.txt
./run.py --image ../output/images/factory/gluon-*-x86-64.img.gz --proto babel
```

`--image` takes a `.img` or a `.img.gz`; a gzipped image is unpacked next
to the archive and reused until the archive changes. Keep one built image
per protocol around (e.g. `images/babel-x86-64.img.gz`) to avoid a
firmware rebuild when switching protocols - rebuild only when the
packages or site config actually change.

Images are built per site config, which selects the routing protocol:
`../contrib/ci/minimal-site` (batman-adv), `../contrib/ci/babel-site`
and `../contrib/ci/olsr-site`.
