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

remove_old_version() {
    echo "Removing old version..."
    [ -f /data/local/tmp/plugin/bin/vpick ] && echo "version:$(/data/local/tmp/plugin/bin/vpick -v)"
    rm -f /data/local/tmp/plugin/bin/vpkd
    rm -f /data/local/tmp/plugin/bin/vpick
    rm -f /data/local/tmp/plugin/etc/init/init.vpkd.rc
    echo "Old version removed"
}

release_new_version() {
    echo "Releasing new version..."
    if [ -f vpick.tgz ]; then
        tar -zxvf vpick.tgz -C /data/local/tmp/plugin/ || { echo "Extraction failed"; exit 1; }
        chmod 755 /data/local/tmp/plugin/etc/init/init.vpkd.rc
        echo "New version released"
    else
        echo "vpick.tgz not found"
        exit 1
    fi
}

start_vpkd() {
    echo "Starting vpkd..."
    start vpkd
    sleep 1
    if pgrep -f "vpkd" > /dev/null; then
        echo "vpkd started successfully"
    else
        nohup /data/local/tmp/plugin/bin/vpkd > /dev/null 2>&1 &
        sleep 2
        if pgrep -f "vpkd" > /dev/null; then
            echo "vpkd started successfully"
        else
            echo "Failed to start vpkd"
            exit 1
        fi
    fi
}

maybe_trigger_vpck() {
    local autotrigger_disabled=$(getprop persist.vpk.autotrigger.disabled 0)
    echo "persist.vpk.autotrigger.disabled=$autotrigger_disabled"
    if [ "$autotrigger_disabled" == 0 ]; then
        ./auto_trigger.sh
        local status=$?
        if [ $status -ne 0 ]; then
            echo "auto_trigger.sh failed with exit code $status"
            return $status
        fi
    fi    
}

print_version() {
    echo ""
    echo "version:$(vpick -v)"
    echo ""
    echo "$(vpick -h)"
    echo ""
}

stop_vpkd
remove_old_version
release_new_version
start_vpkd
maybe_trigger_vpck
print_version


