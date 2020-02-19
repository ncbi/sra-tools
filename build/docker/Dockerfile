# Example:
# % docker build --build-arg BASE_IMAGE=base-debian -t sratoolkit:gcp .
# % docker run --rm sratoolkit:gcp fastq-dump --version
# 
# fastq-dump : 2.10.1 ( 2.10.1-dev )
# 
# % docker image save sratoolkit:gcp | gzip -c >sratoolkit-gcp.tar.gz
# % ls -lh sratoolkit-gcp.tar.gz
# -rw-r--r--  1 ken  staff    87M Nov  1 10:15 sratoolkit-gcp.tar.gz
# % docker image remove sratoolkit:gcp
#
# ... later ...
#
# % gunzip -c sratoolkit-gcp.tar.gz | docker image load
# % docker run --rm sratoolkit:gcp fastq-dump --version
# 
# fastq-dump : 2.10.1 ( 2.10.1-dev )
# 

ARG BASE_IMAGE=base-amazon

FROM debian:stretch AS base-debian
RUN apt-get update -y && apt-get upgrade -y

FROM ubuntu:latest AS base-ubuntu
RUN apt-get update -y && apt-get upgrade -y

FROM amazonlinux:latest AS base-amazon
RUN yum -q -y update

FROM base-debian AS base-debian-builder
RUN apt-get --quiet install -y make cmake git gcc g++ flex bison

FROM base-ubuntu AS base-ubuntu-builder
RUN apt-get --quiet install -y make cmake git gcc g++ flex bison

FROM base-amazon AS base-amazon-builder
RUN yum -q -y install gcc g++ gcc-c++ make cmake git bison flex

FROM ${BASE_IMAGE}-builder AS build
ARG BUILD_STYLE=--without-debug
ARG NGS_BRANCH=engineering
ARG VDB_BRANCH=engineering
ARG SRA_BRANCH=engineering
RUN git clone -b ${NGS_BRANCH} --depth 1 https://github.com/ncbi/ngs.git
RUN git clone -b ${VDB_BRANCH} --depth 1 https://github.com/ncbi/ncbi-vdb.git
RUN git clone -b ${SRA_BRANCH} --depth 1 https://github.com/ncbi/sra-tools.git
WORKDIR /ncbi-vdb
RUN ./configure ${BUILD_STYLE} && make -s >/dev/null 2>&1 || { echo "make failed"; exit 1; }
WORKDIR /ngs
RUN ./configure ${BUILD_STYLE} && make -s -C ngs-sdk >/dev/null 2>&1 || { echo "make failed"; exit 1; }
WORKDIR /sra-tools
RUN ./configure ${BUILD_STYLE} && make -s >/dev/null 2>&1 || { echo "make failed"; exit 1; }
RUN make install

FROM ${BASE_IMAGE}
COPY --from=build /etc/ncbi /etc/ncbi
COPY --from=build /usr/local/ncbi /usr/local/ncbi
ENV PATH=/usr/local/ncbi/sra-tools/bin:${PATH}
