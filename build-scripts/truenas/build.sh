set -x

if [[ ! -d ./build ]]; then
    mkdir build
fi

docker build --tag bookworm-build .
docker run \
    --rm \
    --mount type=bind,source=$(pwd)/build,target=/build \
    --mount type=bind,source=$(pwd)/build-truenas-kmod.sh,target=/build.sh \
    --mount type=bind,source=$(pwd)/extract-truenas-headers.sh,target=/extract-truenas-headers.sh \
    bookworm-build \
    bash -c "cd /build && bash /build.sh $1"
