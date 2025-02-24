:: Simple bat to enable firewall rules for my server change ports here if you dont use port 80

@echo off
echo Enabling Port 80 for Incoming and Outgoing Traffic...

:: Enable Incoming Rules
netsh advfirewall firewall add rule name="Allow Incoming Port 80 TCP" dir=in action=allow protocol=TCP localport=80
netsh advfirewall firewall add rule name="Allow Incoming Port 80 UDP" dir=in action=allow protocol=UDP localport=80

:: Enable Outgoing Rules
netsh advfirewall firewall add rule name="Allow Outgoing Port 80 TCP" dir=out action=allow protocol=TCP localport=80
netsh advfirewall firewall add rule name="Allow Outgoing Port 80 UDP" dir=out action=allow protocol=UDP localport=80

:: Fix the issue with echo
echo Port 80 (TCP and UDP) has been allowed for both Incoming and Outgoing traffic.

pause
