#!/bin/bash

DIRTOTEST=$1

ACCESSION=SRR072810
NAME=${ACCESSION}.wget
HOST=sra-download-internal.ncbi.nlm.nih.gov
URL=$(${DIRTOTEST}/srapath ${ACCESSION})

if echo ${URL} | grep -vq /sdlr/sdlr.fcgi?jwt= ; \
    then \
        ${DIRTOTEST}/vdb-get --reliable -c ./${ACCESSION}.cachetee \
                    ${URL} ${ACCESSION}.dat --progress && \
        wget --no-check-certificate ${URL} -O ./${NAME} && \
        diff ./${NAME} ./${ACCESSION}.dat && \
        rm -f ${ACCESSION}* ; \
    else \
        echo kget test when CE is required is skipped ; \
    fi