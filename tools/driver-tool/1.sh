#!/usr/bin/env bash

export SRATOOLS_IMPERSONATE="fastq-dump"
sratools 1 2 3 --ngc some-ngc-file --kar some-kar-file --perm some-perm-file --location some-location -h
