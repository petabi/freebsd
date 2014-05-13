#!/bin/sh

. /etc/rc.subr

name="pius"
rcvar=pius_enable
pius_flags="-i em1 -o em2"
command="/usr/local/bin/pius"

load_rc_config $name
: ${pius_enable:="yes"}
: ${pius_msg:="pius msg"}

run_rc_command "$1"
