IMAGE="fedora_image"

docker image build -t $IMAGE -f- . << EOF
FROM fedora
RUN dnf update -y
RUN dnf install -y mc htop nano vim cmake git flex bison sudo
RUN dnf install -y iproute iputils samba-client cifs-utils
RUN dnf group install -y "C Development Tools and Libraries"
RUN dnf group install -y "Development Tools"
#RUN dnf install -y java-1.8.0-openjdk-devel.x86_64
RUN dnf install -y mbedtls-devel re2-devel bzip2-devel zlib-devel
RUN dnf install -y libxml2-devel hdf5-devel perl-core libstdc++-static
RUN useradd -b /home -d /home/sra -m -U sra
USER sra
RUN mkdir /home/sra/devel
WORKDIR /home/sra/devel
RUN git clone https://github.com/ncbi/ncbi-vdb.git
RUN git clone https://github.com/ncbi/sra-tools.git
WORKDIR /home/sra/devel/ncbi-vdb
RUN git checkout engineering
WORKDIR /home/sra/devel/sra-tools
RUN git checkout VDB-5712
RUN mkdir -p /home/sra/build/ncbi-vdb /home/sra/build/sra-tools
WORKDIR /home/sra/build/ncbi-vdb
RUN cmake ../../devel/ncbi-vdb
RUN cmake --build . -j
WORKDIR /home/sra/build/sra-tools
RUN cmake -D BUILD_TOOLS_LOADERS=ON -D VDB_LIBDIR=/home/sra/build/ncbi-vdb/lib ../../devel/sra-tools
RUN cmake --build . -j
RUN mkdir /home/sra/.ncbi
RUN cp /home/sra/build/sra-tools/bin/ncbi/default.kfg /home/sra/.ncbi
RUN mv /home/sra/.ncbi/default.kfg /home/sra/.ncbi/user-settings.mkfg
RUN echo "/vdb/schema/paths=\"/home/sra/devel/ncbi-vdb/interfaces\"" >> /home/sra/.ncbi/user-settings.mkfg
RUN mkdir /home/sra/data
EOF

#adjust SRC_LOC to point to the directory containing a valid HDF5-submission
SRC_LOC="/mnt/data/SRA/ACC/PACBIO/HDF5_SRC"
#adjust SRC_DAT to the name of a valid HDF5-submission
SRC_DAT="m130621_024744_42150_c100527702550000001823082709281355_s1_p0.1.bax.h5"

RUSR="--user sra"
RWKDR="--workdir /home/sra"
RHOST="--hostname SRA"
RMNT="-v $SRC_LOC:/home/sra/data"
docker container run -i --rm  $RUSR $RWKDR $RHOST $RMNT $IMAGE /bin/bash << EOF
cd /home/sra/data
/home/sra/build/sra-tools/bin/pacbio-load $SRC_DAT -o /home/sra/SRRXXXXXX -p
EOF
