DOCKER_REPO = neilk3
IMAGE_BUILD_ENV = $(DOCKER_REPO)/splinterdb-build-env
IMAGE_DEV_ENV   = $(DOCKER_REPO)/splinterdb-dev

SPLINTERDB_ROOT = third-party/splinterdb

dev-image:
	docker build -t $(IMAGE_BUILD_ENV) -f $(SPLINTERDB_ROOT)/Dockerfile.build-env $(SPLINTERDB_ROOT)
	docker build -t $(IMAGE_DEV_ENV) -f Dockerfile.dev .

run: dev-image
	docker run -it --rm -v `pwd`/.cache:/splinterdb/build $(IMAGE_DEV_ENV)

submodules:
	git submodule update --init --recursive