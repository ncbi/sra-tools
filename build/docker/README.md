### Building the docker image

#### Example:
```sh
$ make VERSION=2.11.1 build test
$ make VERSION=2.11.1 push clean
```
In general, `docker build` will create an image for the host architecture. It is
easiest to build architecture images on same architecture hosts. After all of
the individual architecture images have been built and pushed, a manifest list
is created with all of them. A manifest list is a small blob of JSON, its
component images are included by their hashes. A 
`docker pull ncbi/sra-tools:latest` will result in a manifest list, and docker 
will pick the image that best suits the host.


#### Makefile targets:

|   Target   | Explanation |
|------------|-------------|
| `build`    | Builds `Dockerfile.build-alpine` and tags the image. |
| `test`     | After building the image, runs `fastq-dump` inside the image. |
| `push`     | Pushes the image to DockerHub (requires user credentials and permissions). |
| `manifest` | Creates and pushes a manifest list with all the platform tags for the version. Run this **after** all the platform images have been pushed. |
| `clean`    | Removes the image from your docker image cache. |

#### General process of the build:
1. Clones `ncbi-vdb`, and `sra-tools` from their github repositories.
2. Builds them.
3. Adds a configuration file to `/etc/.ncbi`.
4. Creates a new image from Alpine Linux and copies the executables and configuration.

The result should be a working standalone installation of the toolkit.

### Check your Docker version

You should have Docker version >= 20.10.0. E.g.
```sh
$ docker --version
Docker version 20.10.8, build 3967b7d
```

#### Basic Docker-ing
```sh
$ docker run -it --rm ncbi/sra-tools
/ # which fastq-dump
/usr/local/ncbi/sra-tools/bin/fastq-dump
/ # cat ${HOME}/.ncbi/user-settings.mkfg
/LIBS/IMAGE_GUID = "aee5f45c-f469-45f1-95f2-b2d2b1c59163"
/libs/cloud/report_instance_identity = "true"
/ # vdb-dump --info SRR000001
acc    : SRR000001
path   : https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000001/SRR000001
size   : 312,527,083
type   : Table
platf  : SRA_PLATFORM_454
SEQ    : 470,985
SCHEMA : NCBI:SRA:_454_:tbl:v2#1.0.7
TIME   : 0x0000000055248a41 (04/08/2015 01:54)
FMT    : SFF
FMTVER : 2.4.5
LDR    : sff-load.2.4.5
LDRVER : 2.4.5
LDRDATE: Feb 25 2015 (2/25/2015 0:0)
```
