FROM ncbi/sra-tools:3.0.0 AS base
RUN apk --no-cache update \
 && apk --no-cache add perl \
 && apk --no-cache add --virtual .build-deps perl-app-cpanminus wget make \
 && cpanm File::chdir \
 && apk del .build-deps
COPY run-test /usr/local/bin
RUN chmod +x /usr/local/bin/run-test
ENV NCBI_VDB_QUALITY=R
ENTRYPOINT [ "perl", "/usr/local/bin/run-test" ]

FROM base AS aws
RUN apk --no-cache add aws-cli
COPY config.aws.pl /usr/local/bin/config.pl

FROM base AS gcp
RUN apk --no-cache add python3 py3-pip py3-cffi py3-cryptography \
 && apk --no-cache add --virtual .build-deps gcc libffi-dev python3-dev linux-headers musl-dev openssl-dev \
 && pip install --upgrade pip \
 && pip install gsutil \
 && apk del .build-deps
COPY config.gsutil.pl /usr/local/bin/config.pl
