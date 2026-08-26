IMAGE_NAME = deckcad-build
TAG = latest

# Context is the repo root, not this directory: the Dockerfile COPYs
# cmake/toolchains/ and Docker/macos-sdk/ for the cross-compile stages.
build:
	docker build -f Dockerfile -t "${IMAGE_NAME}:${TAG}" ..

build-windows:
	docker build -f Dockerfile --target windows-cross -t "${IMAGE_NAME}-windows:${TAG}" ..

build-macos:
	docker build -f Dockerfile --target macos-cross -t "${IMAGE_NAME}-macos:${TAG}" ..

push:
	docker push "${IMAGE_NAME}:${TAG}"
