@echo off
mkdir out
set /p ip_port=<sn.txt
.\bin\adb -s %ip_port% push bin\vpick /data/local/tmp/
.\bin\adb -s %ip_port% shell "chmod 777 /data/local/tmp/vpick"
.\bin\adb -s %ip_port% shell "/data/local/tmp/vpick -b"
.\bin\adb -s %ip_port% pull /data/local/tmp/plugin/meta/vpk out
.\bin\adb -s %ip_port% shell "rm -fr /data/local/tmp/vpick"
.\bin\adb -s %ip_port% shell "rm -fr /data/local/tmp/plugin"
start out\