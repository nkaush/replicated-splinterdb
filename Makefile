DOCKER_REPO = neilk3
IMAGE_BUILD_ENV_BASE = $(DOCKER_REPO)/splinterdb-build-env
IMAGE_RUN_ENV_BASE   = $(DOCKER_REPO)/splinterdb-run-env
IMAGE_BUILD_ENV      = $(DOCKER_REPO)/replicated-splinterdb-dev
IMAGE_RUN_ENV        = $(DOCKER_REPO)/replicated-splinterdb

SPLINTERDB_ROOT = third-party/splinterdb

dev: dev-image
	docker run -it --rm \
		-v `pwd`/include:/work/include \
		-v `pwd`/apps:/work/apps \
		-v `pwd`/src:/work/src \
		-v `pwd`/docker/CMakeLists.txt:/work/CMakeLists.txt \
		-v `pwd`/docker/build:/work/build/build \
		$(IMAGE_BUILD_ENV)

run: # run-image
	docker run -it --network="host" --rm $(IMAGE_RUN_ENV)

dev-image: dev-base-image
	docker build -t $(IMAGE_BUILD_ENV) -f Dockerfile.dev .

run-image: dev-base-image run-base-image
	docker build -t $(IMAGE_RUN_ENV) -f Dockerfile.run .

dev-base-image:
	docker build -t $(IMAGE_BUILD_ENV_BASE) -f $(SPLINTERDB_ROOT)/Dockerfile.build-env $(SPLINTERDB_ROOT)

run-base-image:
	docker build -t $(IMAGE_RUN_ENV_BASE) -f $(SPLINTERDB_ROOT)/Dockerfile.run-env $(SPLINTERDB_ROOT)

submodules:
	git submodule update --init --recursive