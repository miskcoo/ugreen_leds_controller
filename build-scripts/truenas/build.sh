
if [[ ! -d ./build ]]; then
    mkdir build
fi

docker build --tag bookworm-build .
docker run \
    --rm \
    --mount type=bind,source=$(pwd)/build,target=/build \
    --mount type=bind,source=$(pwd)/build-truenas-kmod.sh,target=/build.sh \
    bookworm-build \
    bash -c "cd /build && bash /build.sh $1"
