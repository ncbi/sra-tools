#!/usr/bin/env bash

export SRATOOLS_IMPERSONATE="prefetch"
PAR="A1 A2 A3"
#CMN="--ngc some-ngc-file --kar some-kar-file --perm some-perm-file --location some-location"
#FQ="-N 2 -X 5 --spot-groups 1,2,3 --read-filter split"
#sratools $PAR --aligned-region chr1:20-30 --aligned-region chr2,xx -r chr3 -L 6 -h
sratools $PAR --dryrun
