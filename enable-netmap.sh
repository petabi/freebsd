#!/bin/sh

compile_with_netmap() 
{
    echo "recompile $1 with netmap support"
    local driver="$1"
    cd sys/modules/$driver
    make clean
    make clean && make -DDEV_NETMAP
    make install
}

compile_with_netmap "ixgbe"
compile_with_netmap "igb"


