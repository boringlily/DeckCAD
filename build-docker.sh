#!/bin/bash

IMAGE_TAG=deckcad-build
DOCKER_COMMAND="docker run --rm -v $(pwd):/tmp/ ${IMAGE_TAG}"
COMMAND="bash -c 'cd /tmp/ && ./build-local.sh'"

main() {
  build
}

build() {
  eval "${DOCKER_COMMAND} ${COMMAND}"
}

main
