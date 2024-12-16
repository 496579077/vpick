set /p ip_port=<sn.txt
adb -s %ip_port% push %1 %2