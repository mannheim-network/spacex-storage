#!/bin/bash
function installPrerequisites()
{
    # For basic
    checkAndInstall "${basicsprereq[*]}"

    # For SGX PSW
    checkAndInstall "${sgxpswprereq[*]}"

    # For SGX SDK
    checkAndInstall "${sgxsdkprereq[*]}"

    # For others
    checkAndInstall "${othersprereq[*]}"
}

function installSGXSDK()
{
    # Check SGX SDK
    verbose INFO "Checking SGX SDK..." h
    if [ -d $sgxsdkdir ]; then
        verbose INFO "yes" t
        return 0
    fi
    verbose ERROR "no" t

    # Install SGX SDK
    local res=0
    cd $rsrcdir
    verbose INFO "Installing SGX SDK..." h
    if [ -f "$inteldir/sgxsdk/uninstall.sh" ]; then
        $inteldir/sgxsdk/uninstall.sh &>$ERRFILE
        res=$(($?|$res))
    fi
    execWithExpect_sdk "" "$rsrcdir/$sdkpkg"
    res=$(($?|$res))
    cd - &>/dev/null
    checkRes $res "quit" "success"
}

function installSGXPSW()
{
    # Check SGX PSW
    verbose INFO "Checking SGX PSW..." h
    local inst_pkg_num=0
    for item in $(dpkg -l | grep sgx | awk '{print $3}'); do
        if ! echo $item | grep "2.11.100" &>/dev/null && ! echo $item | grep "1.8.100" &>/dev/null; then
            break
        fi
        ((inst_pkg_num++))
    done
    if [ $inst_pkg_num -lt ${#sgxpswlibs[@]} ]; then
        verbose ERROR "no" t
    else
        verbose INFO "yes" t
        return 0
    fi

    # Install SGX PSW
    local res=0
    > $SYNCFILE
    setTimeWait "$(verbose INFO "Installing SGX PSW..." h)" $SYNCFILE &
    toKillPID[${#toKillPID[*]}]=$!
    echo 'deb [arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu bionic main' | tee /etc/apt/sources.list.d/intel-sgx.list &>$ERRFILE
    res=$(($?|$res))
    wget -qO - https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key | apt-key add - &>>$ERRFILE
    res=$(($?|$res))
    apt-get update &>>$ERRFILE
    res=$(($?|$res))
    apt-get install -y --allow-downgrades ${sgxpswlibs[@]} &>>$ERRFILE
    res=$(($?|$res))
    /opt/intel/sgx-aesm-service/cleanup.sh &>>$ERRFILE
    res=$(($?|$res))
    /opt/intel/sgx-aesm-service/startup.sh &>>$ERRFILE
    res=$(($?|$res))
    checkRes $res "quit" "success" "$SYNCFILE"
}

function installSGXDRIVER()
{
    # Check SGX driver
    verbose INFO "Checking SGX driver..." h
    if [ -d $sgxdriverdir ]; then
        verbose INFO "yes" t
        return 0
    fi
    verbose ERROR "no" t

    # Install SGX driver
    local res=0
    cd $rsrcdir
    verbose INFO "Installing SGX driver..." h
    $rsrcdir/$driverpkg &>$ERRFILE
    res=$(($?|$res))
    cd - &>/dev/null
    checkRes $res "quit" "success"
}

function installSGXSSL()
{
    verbose INFO "Checking SGX SSL..." h
    if [ -d "$sgxssldir" ]; then
        verbose INFO "yes" t
        return
    fi
    verbose ERROR "no" t
    
    local ret_l=0

    # Install SGXSDK
    > $SYNCFILE
    setTimeWait "$(verbose INFO "Installing SGX SSL..." h)" $SYNCFILE &
    toKillPID[${#toKillPID[*]}]=$!
    cd $rsrcdir
    tar -xvf toolset.tar.gz &>$ERRFILE
    mkdir -p /usr/local/bin
    cp $rsrcdir/toolset/* /usr/local/bin/
    rm -rf $rsrcdir/toolset 
    sgxssltmpdir=$rsrcdir/$(unzip -l $sgxsslpkg | awk '{print $NF}' | awk -F/ '{print $1}' | grep intel | head -n 1)
    sgxssl_openssl_source_dir=$sgxssltmpdir/openssl_source
    unzip $sgxsslpkg &>$ERRFILE
    if [ $? -ne 0 ]; then
        verbose "failed" t
        exit 1
    fi
    cd - &>/dev/null

    # build SGX SSL
    cd $sgxssltmpdir/Linux
    cp $opensslpkg $sgxssl_openssl_source_dir
    if [ "$1" == "0" ]; then
        make all test &>$ERRFILE && make install &>$ERRFILE
    else
        make all &>$ERRFILE && make install &>$ERRFILE
    fi
    checkRes $? "quit" "success" "$SYNCFILE"
    cd - &>/dev/null

    if [ x"$sgxssltmpdir" != x"/" ]; then
        rm -rf $sgxssltmpdir
    fi
}

function installBOOST()
{
    verbose INFO "Checking boost..." h
    if [ -d "$boostdir" ]; then
        if [ ! -s "$spacexldfile" ]; then
            echo "$boostdir/lib" > $spacexldfile
            ldconfig
        fi
        if ! find /usr/local/include/boost -name "core.hpp" | grep "core" &>/dev/null; then
            ln -s $boostdir/include/boost /usr/local/include/boost &>/dev/null
        fi
        verbose INFO "yes" t
        return
    fi
    verbose ERROR "no" t
    mkdir -p $boostdir

    # Install boost beast
    > $SYNCFILE
    setTimeWait "$(verbose INFO "Installing boost..." h)" $SYNCFILE &
    toKillPID[${#toKillPID[*]}]=$!
    cd $rsrcdir
    local tmpboostdir=$rsrcdir/boost
    mkdir $tmpboostdir
    tar -xvf $boostpkg -C $tmpboostdir --strip-components=1 &>$ERRFILE
    if [ $? -ne 0 ]; then
        verbose "failed" t
        exit 1
    fi
    cd - &>/dev/null

    # Build boost
    cd $tmpboostdir
    ./bootstrap.sh &>$ERRFILE
    if [ $? -ne 0 ]; then
        verbose "failed" t
        exit 1
    fi
    ./b2 install --prefix=$boostdir threading=multi -j$((coreNum*2)) &>$ERRFILE
    checkRes $? "quit" "success" "$SYNCFILE"
    cd - &>/dev/null

    # Set and refresh ldconfig
    echo "$boostdir/lib" > $spacexldfile
    ldconfig
    ln -s $boostdir/include/boost /usr/local/include/boost

    if [ x"$tmpboostdir" != x"/" ]; then
        rm -rf $tmpboostdir
    fi
}

function checkAndInstall()
{
    for dep in $1; do
        verbose INFO "Checking $dep..." h
        dpkg -l | grep "\b$dep\b" &>/dev/null
        checkRes $? "return" "yes"
        if [ $? -ne 0 ]; then
            > $SYNCFILE
            setTimeWait "$(verbose INFO "Installing $dep..." h)" $SYNCFILE &
            toKillPID[${#toKillPID[*]}]=$!
            apt-get install -y $dep &>$ERRFILE
            checkRes $? "quit" "success" "$SYNCFILE"
        fi
    done
}

function execWithExpect()
{
    local cmd=$1
    local pkgPath=$2
expect << EOF > $TMPFILE
    set timeout $instTimeout
    spawn sudo $cmd $pkgPath
    expect "password"        { send "$passwd\n"  }
    expect eof
EOF
    ! cat $TMPFILE | grep "error\|ERROR" &>/dev/null
    return $?
}

function execWithExpect_sdk()
{
    local cmd=$1
    local pkgPath=$2
expect << EOF > $TMPFILE
    set timeout $instTimeout
    spawn $cmd $pkgPath
    expect "yes/no"          { send "no\n"  }
    expect "to install in :" { send "$inteldir\n" }
    expect eof
EOF
    cat $TMPFILE | grep successful &>/dev/null
    return $?
}

function success_exit()
{
    rm -f $TMPFILE

    rm -f $SYNCFILE &>/dev/null

    # Kill alive useless sub process
    for el in ${toKillPID[@]}; do
        if ps -ef | grep -v grep | grep $el &>/dev/null; then
            kill -9 $el
        fi
    done

    # delete sgx ssl temp directory
    if [ x"$sgxssltmpdir" != x"" ] && [ x"$sgxssltmpdir" != x"/" ]; then
        rm -rf $sgxssltmpdir
    fi

    #kill -- -$selfPID
}


usage() {
    echo "Usage:"
        echo "    $0 -h                      Display this help message."
        echo "    $0 [options]"
    echo "Options:"
    echo "     -u no test"

    exit 1;
}

############## MAIN BODY ###############
# basic variable
basedir=$(cd `dirname $0`;pwd)
scriptdir=$basedir
### Import parameters {{{
. $scriptdir/utils.sh
### }}}
TMPFILE=$scriptdir/tmp.$$
ERRFILE=$scriptdir/err.log
rsrcdir=$scriptdir/../resource
spacexdir=/opt/spacex
spacestoragedir=$spacexdir/spacex-storage
realsworkerdir=$spacestoragedir/$(cat $scriptdir/../VERSION | head -n 1)
spacextooldir=$spacexdir/tools
inteldir=/opt/intel
sgxsdkdir=$inteldir/sgxsdk
sgxdriverdir=$inteldir/sgxdriver
sgxssldir=$inteldir/sgxssl
boostdir=$spacextooldir/boost
sgxssltmpdir=""
selfPID=$$
SYNCFILE=$scriptdir/.syncfile
res=0
# Environment related
spacexldfile="/etc/ld.so.conf.d/spacex.conf"
uid=$(stat -c '%U' $scriptdir)
coreNum=$(cat /proc/cpuinfo | grep processor | wc -l)
# Control configuration
instTimeout=30
toKillPID=()
# Files
sdkpkg=sgx_linux_x64_sdk_2.11.100.2.bin
driverpkg=sgx_linux_x64_driver_2.6.0_b0a445b.bin
sgxsslpkg=$rsrcdir/intel-sgx-ssl-master.zip
opensslpkg=$rsrcdir/openssl-1.1.1g.tar.gz
openssldir=$rsrcdir/$(echo openssl-1.1.1g.tar.gz | grep -Po ".*(?=\.tar)")
boostpkg=$rsrcdir/boost_1_70_0.tar.gz
# SGX PSW package
sgxpswlibs=(libsgx-ae-epid=2.11.100.2-bionic1 libsgx-ae-le=2.11.100.2-bionic1 \
libsgx-ae-pce=2.11.100.2-bionic1 libsgx-ae-qe3=1.8.100.2-bionic1 libsgx-aesm-ecdsa-plugin=2.11.100.2-bionic1 \
libsgx-aesm-epid-plugin=2.11.100.2-bionic1 libsgx-aesm-launch-plugin=2.11.100.2-bionic1 \
libsgx-aesm-pce-plugin=2.11.100.2-bionic1 libsgx-aesm-quote-ex-plugin=2.11.100.2-bionic1 \
libsgx-enclave-common=2.11.100.2-bionic1 libsgx-epid=2.11.100.2-bionic1 libsgx-launch=2.11.100.2-bionic1 \
libsgx-pce-logic=1.8.100.2-bionic1 libsgx-qe3-logic=1.8.100.2-bionic1 libsgx-quote-ex=2.11.100.2-bionic1 \
libsgx-urts=2.11.100.2-bionic1 sgx-aesm-service=2.11.100.2-bionic1)
# SGX prerequisites
basicsprereq=(expect kmod unzip linux-headers-`uname -r`)
sgxsdkprereq=(build-essential python)
sgxpswprereq=(libssl-dev libcurl4-openssl-dev libprotobuf-dev wget)
othersprereq=(libboost-all-dev libleveldb-dev openssl)
# Spacex related
spacex_env_file=$realsworkerdir/etc/environment
sgx_env_file=/opt/intel/sgxsdk/environment

disown -r

. $scriptdir/utils.sh

#trap "success_exit" INT
trap "success_exit" EXIT

if [ $(id -u) -ne 0 ]; then
    verbose ERROR "Please run with sudo!"
    exit 1
fi

UNTEST=0

while getopts ":hu" opt; do
    case ${opt} in 
        h)
            usage
            ;;
        u)
            UNTEST=1
            ;;
        ?)
            echo "Invalid Option: -$OPTARG" 1>&2
            exit 1
            ;;
    esac
done

# Installing Prerequisites
installPrerequisites

# Installing SGX SDK
installSGXSDK

# Installing SGX driver
installSGXDRIVER

# Installing SGX PSW
installSGXPSW

# Installing SGX SSL
installSGXSSL $UNTEST

# Installing BOOST
installBOOST
