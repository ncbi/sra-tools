# Make file for building SRA Toolkit docker image

ifndef VERSION
$(error VERSION needs to be set, e.g. make VERSION=2.11.1 build)
endif

DOCKER_ARCH := $(shell docker info | perl -ne'print "$$1\n" if /Architecture:\s*(\w+)/')
BRANCH?=${VERSION}rc
REPO=ncbi/sra-tools
TAG=${DOCKER_ARCH}-${VERSION}

.DEFAULT: build
.PHONY: build buildx push manifest clean test help

help:
	@ echo "targets are: build, test, push, manifest, and clean"

build:
	docker pull alpine:latest
	docker build --build-arg VDB_BRANCH=${BRANCH} --tag ${REPO}:${TAG} -f Dockerfile.build-alpine .

build-debug:
	docker pull alpine:latest
	docker build --build-arg VDB_BRANCH=${BRANCH} --build-arg CMAKE_BUILD_TYPE=Debug --tag ${REPO}:${TAG}-debug -f Dockerfile.build-alpine .

test:
	docker run -it --rm ${REPO}:${TAG} fastq-dump -X 5 -Z SRR000001

push:
	docker push ${REPO}:${TAG}
	@ echo Remember to make VERSION=${VERSION} manifest once all the images are built.

manifest:
	# Need to remove any old manifests with the same tags we are creating
	docker manifest rm ${REPO}:latest || true
	docker manifest rm ${REPO}:${VERSION} || true
	docker manifest create ${REPO}:${VERSION} $(shell perl find-parts.pl ${VERSION})
	docker manifest push ${REPO}:${VERSION}
	docker manifest create ${REPO}:latest ${REPO}:x86_64-${VERSION} ${REPO}:aarch64-${VERSION}
	docker manifest push ${REPO}:latest

clean:
	docker image rm ${REPO}:${TAG}
	docker manifest rm ${REPO}:latest
	docker manifest rm ${REPO}:${VERSION}

buildx:
	docker build \
	    --platform=linux/amd64,linux/arm64 \
	    --build-arg VDB_BRANCH=${BRANCH} \
	    --tag ${REPO}:${VERSION} \
	    --tag ${REPO}:latest \
	    --file Dockerfile.build-alpine \
	    --pull \
	    --push \
	    .
