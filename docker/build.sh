#! /usr/bin/env bash
# build spacex-storage docker image

usage() {
    echo "Usage:"
        echo "    $0 -h                      Display this help message."
        echo "    $0 [options]"
    echo "Options:"
    echo "     -p publish image"
    echo "     -m build mode(dev or prod)"

      exit 1;
}

PUBLISH=0

while getopts ":hpm:" opt; do
    case ${opt} in
        h)
            usage
            ;;
        p)
            PUBLISH=1
            ;;
        m)
            XSTORAGE_MODE=$OPTARG
            ;;
        ?)
            echo "Invalid Option: -$OPTARG" 1>&2
            exit 1
            ;;
    esac
done

VER=$(cat VERSION | head -n 1)
IMAGEID="mannheim-network/spacex-storage:$VER"

echo "building spacex-storage runner image $IMAGEID"
if [ "$PUBLISH" -eq "1" ]; then
    echo "will publish after build"
fi

make clean
if [ x"$XSTORAGE_MODE" != x"prod" ]; then
    XSTORAGE_MODE="dev"
fi
docker build -f docker/runner/Dockerfile -t $IMAGEID --build-arg BUILD_MODE=$XSTORAGE_MODE .

if [ "$?" -ne "0" ]; then
    echo "spacex-storage build failed!"
    exit 1
fi

echo "mannheim-network/spacex-storage build success"
if [ "$PUBLISH" -eq "1" ]; then
    echo "will publish image to $IMAGEID"
    docker push $IMAGEID
fi
