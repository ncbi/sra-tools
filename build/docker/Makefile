# Make file for building SRA Toolkit docker image

ifndef VERSION
$(error VERSION needs to be set, e.g. make VERSION=2.11.1 build)
endif

DOCKER_ARCH := $(shell docker info | perl -ne'print "$$1\n" if /Architecture:\s*(\w+)/')
BRANCH?=${VERSION}rc
TAG=${DOCKER_ARCH}-${VERSION}

.DEFAULT: build
.PHONY: build push clean scan test

build:
	docker pull alpine:latest
	docker build --build-arg VDB_BRANCH=${BRANCH} --tag ncbi/sra-tools:${TAG} -f Dockerfile.build-alpine .

test:
	docker run -it --rm ncbi/sra-tools:${TAG} fastq-dump -X 5 -Z SRR000001

scan:
	docker scan --file Dockerfile.build-alpine ncbi/sra-tools:${TAG}

push:
	docker push ncbi/sra-tools:${TAG}
	@ echo docker push ncbi/sra-tools:latest

clean:
	docker image rm ncbi/sra-tools ncbi/sra-tools:${TAG}
