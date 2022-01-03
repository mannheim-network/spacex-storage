#! /usr/bin/env bash

spacexdir=/opt/spacex
version=$(cat /spacex-storage/VERSION | head -n 1)
spacexstoragedir=$spacexdir/spacex-storage/$version
spacex_env_file=$spacexstoragedir/etc/environment
inteldir=/opt/intel

echo "Starting curst sworker $version"
source $spacex_env_file

wait_time=10
echo "Wait $wait_time seconds for aesm service fully start"
/opt/intel/sgx-aesm-service/aesm/linksgx.sh
/bin/mkdir -p /var/run/aesmd/
/bin/chown -R aesmd:aesmd /var/run/aesmd/
/bin/chmod 0755 /var/run/aesmd/
/bin/chown -R aesmd:aesmd /var/opt/aesmd/
/bin/chmod 0750 /var/opt/aesmd/
NAME=aesm_service AESM_PATH=/opt/intel/sgx-aesm-service/aesm LD_LIBRARY_PATH=/opt/intel/sgx-aesm-service/aesm /opt/intel/sgx-aesm-service/aesm/aesm_service
sleep $wait_time

ps -ef | grep aesm

echo "Run spacex-storage with arguments: $ARGS"
/opt/spacex/spacex-storage/$version/bin/spacex-storage $ARGS
