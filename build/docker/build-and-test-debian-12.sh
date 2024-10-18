TAG="engineering_debian"
SCRIPT="build_and_test.sh"
BRANCH_NCBI_VDB="engineering"
BRANCH_SRA_TOOLS="engineering"

function make_image {
docker image build -t $TAG -f- . << EOF
FROM debian
RUN apt-get update
RUN apt-get install -y mc htop nano vim cmake git flex bison build-essential \
default-jdk gdb valgrind libmbedtls-dev libre2-dev libbz2-dev libz-dev \
libxml2-dev libhdf5-dev apt-utils python3 pip
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y locales
RUN sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen && \
    dpkg-reconfigure --frontend=noninteractive locales && \
    update-locale LANG=en_US.UTF-8
ENV LANG en_US.UTF-8
RUN useradd -b /home -d /home/sra -m -U sra
USER sra
RUN echo "export LC_ALL=C; unset LANGUAGE" >> /home/sra/.profile
RUN mkdir /home/sra/devel
WORKDIR /home/sra
RUN echo "set -e\n\
cd /home/sra/devel\n\
git clone https://github.com/ncbi/ncbi-vdb.git\n\
cd /home/sra/devel/ncbi-vdb\n\
git checkout $BRANCH_NCBI_VDB\n\
git pull\n\
./configure\n\
make -j\n\
make test -j\n\
cd /home/sra/devel\n\
git clone https://github.com/ncbi/sra-tools.git\n\
cd /home/sra/devel/sra-tools\n\
git checkout $BRANCH_SRA_TOOLS\n\
git pull\n\
./configure\n\
make all -j\n\
make BUILD_TOOLS_INTERNAL=ON BUILD_TOOLS_LOADERS=ON BUILD_TOOLS_TEST_TOOLS=ON test -j\n" >> $SCRIPT
RUN chmod +x $SCRIPT
EOF
}

USR="--user sra"
WKD="--workdir /home/sra"
HSN="--hostname SRA"
TZ1="-v /etc/timezone:/etc/timezone:ro"
TZ2="-v /etc/localtime:/etc/localtime:ro"

#the container run tests on ncbi-vdb and sra-tools
function run_container {
docker run -i --rm $USR $WKD $HSN $TZ1 $TZ2 $TAG /bin/bash -c /home/sra/$SCRIPT

}

#this container just logs you in as the user "sra" - to manually test/fix problems
function run_container2 {
docker run -it --rm $USR $WKD $HSN $TZ1 $TZ2 $TAG
}

make_image
run_container
#run_container2
