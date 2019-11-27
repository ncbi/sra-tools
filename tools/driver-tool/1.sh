#!/usr/bin/env bash

export SRATOOLS_IMPERSONATE="fastq-dump"
PAR="1 2 3"
CMN="--ngc some-ngc-file --kar some-kar-file --perm some-perm-file --location some-location"
FQ="-N 2 -X 5 --spot-groups 1,2,3 --read-filter split"
sratools $PAR $CMN $FQ -h
