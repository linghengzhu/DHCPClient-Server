# DHCPClient-Server
It's the coursework project for Internet Applications course.
<br>How to use the server?
  1. Open XShell, type "mkdir XX(file name defined by yourself)"
  2. Type'rz', copy the .cpp file into the virtual machine
  3. type "cd .." in XShell, get back to the upper level folder
  4. type 'rz' again, copy the .config file into the virtual machine
  5. cd enter self-defined folder
  6. type "g++ DHCPSVR.cpp -lpthread -lm" to compile (call pthread and math)
  7. ./aout excute the program
  
<br>How to use the client?
<br>  //preparing
<br>  gcc -o DHCPclient DHCPclient.c
<br>  sudo killall dhclient
<br>  sudo ./DHCPclient --renew
<br>  sudo ./DHCPclient --inform
<br>  //wrong ip
<br>  sudo ./DHCPclient --renew 192.168.1.1
<br>  //auto renew
<br>  sudo ./DHCPclient --interact
<br>  sudo ./DHCPclient --release
<br>  //standard procedure (non-auto)
<br>  sudo ./DHCPclient --default
<br>  //the log file generated is client.log
