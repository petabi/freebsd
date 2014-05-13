#!/bin/sh

make_install_nic_driver_w_netmap() 
{
    echo "make install $1 with netmap support"
    local driver="$1"
    cd sys/modules/$driver
    make clean && make -DDEV_NETMAP && make install
}

make_install_nic_driver_w_netmap "ixgbe"
make_install_nic_driver_w_netmap "igb"
