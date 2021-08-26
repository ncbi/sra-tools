### Building the docker image

#### Example:
```
$ make VERSION=2.11.1 build test scan
$ make VERSION=2.11.1 push clean
```

#### Makefile targets:

Target | Explanation
------ | -----------
build | Builds `Dockerfile.build-alpine` and tags the image
test | After building the image, runs `fastq-dump` inside the image
scan | Runs vulnerability scan
push | Pushes the image to dockerhub (requires user credentials and permissions)
clean | Removes the image from your docker image cache

#### General process of the build:
1. Clones `ngs`, `ncbi-vdb`, and `sra-tools` from their github repositories.
2. Builds them.
3. Adds a configuration file to `/root/.ncbi`.
4. Creates a new image from Alpine Linux and copies the executables and configuration.

The result should be a working standalone installation of the toolkit.
