# Building the LED kernel module for TrueNAS SCALE

Run:

```
bash build.sh TrueNAS-SCALE-Goldeye/25.10.4
```

The argument is the version path as it appears under
<https://download.truenas.com/>. The resulting `led-ugreen.ko` is written to
`build/<version-path>/`.

## How it works

1. Download the TrueNAS-SCALE `.update` file (a GPG-signed squashfs image) and
   its `.sig` for the requested version.
2. Verify the signature against the iX SecTeam key, then extract the kernel
   headers package from the image with `extract-truenas-headers.sh`.
3. Build `led-ugreen.ko` against those headers inside a Debian container.

The headers come from the `.update` image because the per-release
`packages/Packages.gz` index that earlier builds relied on is not published for
every version (e.g. it is missing for 25.10.4). An `.update` file, by contrast,
is published for every release.
