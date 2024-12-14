#!/bin/sh

mkdir -p /data/local/tmp/plugin
tar -zxvf vpick.tar.gz -C /data/local/tmp/plugin/
chmod 644 /data/local/tmp/plugin/etc/init/init.vpkd.rc