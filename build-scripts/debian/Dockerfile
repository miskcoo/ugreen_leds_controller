# Parametrizable Debian version, defaults to bookworm
ARG DEBIAN_VERSION=trixie
FROM --platform=linux/amd64 debian:${DEBIAN_VERSION}

RUN set -ex \
    && sed -i -- 's/Types: deb/Types: deb deb-src/g' /etc/apt/sources.list.d/debian.sources \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
               build-essential \
               cdbs \
               devscripts \
               equivs \
               fakeroot \
               wget \
               git \
               debhelper \
               dh-make \
               dh-dkms \
               dkms \
    && apt-get clean \
    && rm -rf /tmp/* /var/tmp/*

# Install g++ for compiling C++ utilities
RUN apt-get update && apt-get install -y g++ && apt-get clean

WORKDIR /build


