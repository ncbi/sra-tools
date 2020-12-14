# Make file for building SRA Toolkit docker image

BRANCH?=engineering

clean:
	docker image rm ncbi/sra-tools ncbi/sra-tools:${VERSION}

build:
	docker build --build-arg VDB_BRANCH=${BRANCH} -t ncbi/sra-tools:${VERSION} -f Dockerfile.build-alpine .
	docker tag ncbi/sra-tools:${VERSION} ncbi/sra-tools:latest

publish:
	docker push ncbi/sra-tools:${VERSION}
	docker push ncbi/sra-tools:latest

.DEFAULT: publish
.PHONY: build publish clean
