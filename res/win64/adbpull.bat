set /p ip_port=<sn.txt
adb -s %ip_port% pull %1 %2