# spacex storage env image
FROM ubuntu:18.04

# install build depends
RUN apt-get update
RUN apt-get install -y wget expect kmod unzip
RUN apt-get install -y libboost-all-dev libleveldb-dev build-essential
RUN apt-get install -y linux-headers-`uname -r` libssl-dev curl libprotobuf-dev libcurl4-openssl-dev
ADD resource /spacex-storage-env/resource
ADD scripts/*.sh /spacex-storage-env/scripts/
ADD VERSION /spacex-storage-env/
RUN /spacex-storage-env/scripts/install_deps.sh -u
