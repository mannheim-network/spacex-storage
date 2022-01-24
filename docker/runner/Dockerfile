# spacex storage image
FROM mannheim-network/spacex-storage-env:1.1.0

ARG BUILD_MODE

ADD scripts /spacex-storage/scripts
ADD src /spacex-storage/src
ADD test /spacex-storage/test
ADD VERSION /spacex-storage/VERSION
ADD Makefile /spacex-storage/Makefile
ADD buildenv.mk /spacex-storage/buildenv.mk
RUN /spacex-storage/scripts/install.sh -d -m ${BUILD_MODE}
ADD docker/runner/start_xstorage.sh /
CMD /start_xstorage.sh
