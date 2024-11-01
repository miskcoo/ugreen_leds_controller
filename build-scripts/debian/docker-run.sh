

set -x

if [[ ! -d ./build ]]; then
    mkdir build
fi

docker build --tag bookworm-build .
docker run \
    --rm \
    --mount type=bind,source=$(pwd)/build,target=/build \
    bookworm-build \
    bash -c "cd /build && bash /build.sh $1"
