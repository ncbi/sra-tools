# Make file for building SRA Toolkit docker image

BRANCH?=engineering

.DEFAULT: build
.PHONY: build push clean scan

build:
	docker pull alpine:latest
	docker build --build-arg VDB_BRANCH=${BRANCH} -t ncbi/sra-tools:${VERSION} -f Dockerfile.build-alpine .
	docker tag ncbi/sra-tools:${VERSION} ncbi/sra-tools:latest

scan:
	docker scan --file Dockerfile.build-alpine ncbi/sra-tools:${VERSION}

push:
	docker push ncbi/sra-tools:${VERSION}
	docker push ncbi/sra-tools:latest

clean:
	docker image rm ncbi/sra-tools ncbi/sra-tools:${VERSION}
