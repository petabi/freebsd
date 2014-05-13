#!/bin/sh
# PROVIDE :install_pkg
# BEFORE: LOGIN
# REQUIRE: bgfsck
. /etc/rc.subr

name="install_pkg"
rcvar=install_pkg_enable
start_cmd="${name}_start"
stop_cmd=":"

install_pkg_start()
{
    mount -uw /

    mkdir -p /mnt/pkg
    # restore pkg database
    cp -Rp /mnt/pkg /var/db
    env ASSUME_ALWAYS_YES=YES pkg bootstrap
    pkg install -y mregex
    pkg install -y session
    pkg install -y pius
    mount -ur /
    # save pkg database
    cp -Rp /var/db/pkg /mnt/
}

load_rc_config $name
: ${install_pkg_enable:="yes"}

run_rc_command "$1"
