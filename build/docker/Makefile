# Make file for building SRA Toolkit docker image

ifndef VERSION
$(error VERSION needs to be set, e.g. make VERSION=2.11.1 build)
endif

DOCKER_ARCH := $(shell docker info | perl -ne'print "$$1\n" if /Architecture:\s*(\w+)/')
BRANCH?=${VERSION}rc
REPO=ncbi/sra-tools
TAG=${DOCKER_ARCH}-${VERSION}

.DEFAULT: build
.PHONY: build push manifest clean test help

help:
	@ echo "targets are: build, test, push, manifest, and clean"

build:
	docker pull alpine:latest
	docker build --build-arg VDB_BRANCH=${BRANCH} --tag ${REPO}:${TAG} -f Dockerfile.build-alpine .

test:
	docker run -it --rm ${REPO}:${TAG} fastq-dump -X 5 -Z SRR000001

push:
	docker push ${REPO}:${TAG}
	@ echo remember to make VERSION=${VERSION} manifest 

manifest:
	docker manifest create ${REPO}:${VERSION} ${REPO}:x86_64-${VERSION} ${REPO}:aarch64-${VERSION}
	docker manifest push ${REPO}:${VERSION}
#	docker manifest create ${REPO}:latest ${REPO}:x86_64-${VERSION} ${REPO}:aarch64-${VERSION}
#	docker manifest push ${REPO}:latest

clean:
	docker image rm ${REPO}:${TAG} ${REPO}:${VERSION}
