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
scan | Runs vulnerability scan (requires user credentials)
push | Pushes the image to dockerhub (requires user credentials and permissions)
clean | Removes the image from your docker image cache

#### General process of the build:
1. Clones `ngs`, `ncbi-vdb`, and `sra-tools` from their github repositories.
2. Builds them.
3. Adds a configuration file to `/root/.ncbi`.
4. Creates a new image from Alpine Linux and copies the executables and configuration.

The result should be a working standalone installation of the toolkit.

### Check your Docker version

You should have Docker version >= 20.10.0. E.g.
```
$ docker --version
Docker version 20.10.8, build 3967b7d
```

#### Basic Docker-ing
```
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
