# Make file for building SRA Toolkit docker image

ifndef VERSION
$(error VERSION needs to be set, e.g. VERSION=2.11.1)
endif

BRANCH?=${VERSION}rc

.DEFAULT: build
.PHONY: build push clean scan test

build:
	docker pull alpine:latest
	docker build --build-arg VDB_BRANCH=${BRANCH} -t ncbi/sra-tools:${VERSION} --no-cache -f Dockerfile.build-alpine .
	docker tag ncbi/sra-tools:${VERSION} ncbi/sra-tools:latest

test:
	docker run -it --rm ncbi/sra-tools:${VERSION} fastq-dump -X 5 -Z SRR000001

scan:
	docker scan --file Dockerfile.build-alpine ncbi/sra-tools:${VERSION}

push:
	docker push ncbi/sra-tools:${VERSION}
	docker push ncbi/sra-tools:latest

clean:
	docker image rm ncbi/sra-tools ncbi/sra-tools:${VERSION}
