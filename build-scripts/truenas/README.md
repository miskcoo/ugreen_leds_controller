# Building the LED kernel module for TrueNAS SCALE

Run:

```
bash build.sh TrueNAS-SCALE-Goldeye/25.10.4
```

The argument is the version path as it appears under
<https://download.truenas.com/>. The resulting `led-ugreen.ko` is written to
`build/<version-path>/`.

The default source ref is `master`. To build a specific tag, pass it as the
second argument:

```
bash build.sh TrueNAS-SCALE-Goldeye/25.10.4 v0.4-beta
```

Tag outputs are written under `build/tags/<tag>/<version-path>/`, for example:

```
build/tags/v0.4-beta/TrueNAS-SCALE-Goldeye/25.10.4/led-ugreen.ko
```

To build every supported TrueNAS version for multiple refs, use
`UGREEN_SOURCE_REFS`. CI builds `master`, the latest stable tag, and the
current beta tag:

```
UGREEN_SOURCE_REFS="master v0.3 v0.4-beta" bash build-all.sh
```

`master` keeps the existing output layout under `build/<version-path>/`.
Every non-`master` ref is treated as a tag build and written under
`build/tags/<ref>/<version-path>/`. CI can set `TRUENAS_BUILD_ROOT` to write
artifacts into another checkout.

## How it works

1. Download the TrueNAS-SCALE `.update` file (a GPG-signed squashfs image) and
   its `.sig` for the requested version.
2. Verify the signature against the iX SecTeam key, then extract the kernel
   headers package from the image with `extract-truenas-headers.sh`.
3. Clone this repository, checkout the requested source ref, and build
   `led-ugreen.ko` against those headers inside a Debian container.

The headers come from the `.update` image because the per-release
`packages/Packages.gz` index that earlier builds relied on is not published for
every version (e.g. it is missing for 25.10.4). An `.update` file, by contrast,
is published for every release.
