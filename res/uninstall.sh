#!/bin/sh

stop_vpkd() {
    stop vpkd
    sleep 1
    if pgrep -f "vpkd" > /dev/null; then
        echo "Stopping vpkd..."
        pkill -f "vpkd"
        sleep 1
        if pgrep -f "vpkd" > /dev/null; then
            echo "Failed to stop vpkd, forcing termination..."
            pkill -9 -f "vpkd"
        fi
        echo "vpkd stopped."
    else
        echo "vpkd is not running."
    fi
}

uninstall_vpkd() {
    echo "Removing files ..."
    rm -fr /data/local/tmp/plugin/bin/vpkd
    rm -fr /data/local/tmp/plugin/bin/vpick
    rm -fr /data/local/tmp/plugin/etc/init/init.vpkd.rc
    rm -fr /data/local/tmp/plugin/meta/vpk
    echo "uninstall finished"
}

stop_vpkd
uninstall_vpkd