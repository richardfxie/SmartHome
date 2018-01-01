# SmartHome
# location of files
/etc/system/system/homebridge.service

/etc/default/homebridge

# Installation steps:

1.	Install node js and hombebridge.
2.	Install PiThermostat plugin. sudo npm install –g homebridge-pi-thermost
3.	Copy index.js in the Appendix to /usr/local/lib/node_modules/homebridge-pi-thermostat
4.	Enable one-wire interface. At the command line,  enter 
sudo vi /boot/config.txt, then add this to the bottom of the file:
dtoverlay=w1–gpio

5.	Restart the system using 
sudo shutdown –r now 
6.	Assume thermostat.c is under /home/pi. Compile thermostat.c using 

gcc -Wall -O2 -o thermostat thermostat.c -lpthread -lwiringPi

7.	An updated homebridge thermostat plugin based on https://github.com/jeffmcfadden/homebridge-pi-thermostat-accessory/blob/master/index.js is built to send HomeKit commands to the REST APIs provided by thermostat program described here. See appendix for code listing for the updated plugin index.js

8.	Create /etc/default/homebridge file with the below:

#Defaults / Configuration options for homebridge

#The following settings tells homebridge where to find the config.json file and where to persist the data (i.e. pairing and others)

HOMEBRIDGE_OPTS=-U /var/homebridge

#If you uncomment the following line, homebridge will log more

#You can display this via systemd's journalctl: journalctl -f -u homebridge

#DEBUG=*




9.	Create homebridge user to run homebridge service. 
 sudo useradd --system homebridge

10.	Add homebridge to gpio group.  sudo usermod -G gpio homebridge

11.	Create a home directory for homebridge. sudo mkdir /var/homebridge

12.	Copy thermostat executable to /var/homebridge. 

sudo cp ~/thermostat /var/homebridge

13.	Create startThermostat.sh, with the following content:

#!/bin/sh -
/var/homebridge/thermostat
/usr/local/bin/homebridge $HOMEBRIDGE_OPTS

14.	Change permission. sudo chmod –R 0777 /var/homebridge
15.	Create homebridge.service settings in /etc/system/system below to ensure that startThermostat.sh is executed during system startup.
[Unit]
Description=Node.js HomeKit Server
After=syslog.target network-online.target

[Service]
Type=simple
User=homebridge
EnvironmentFile=/etc/default/homebridge
ExecStart=/var/homebridge/startThermostat.sh
Restart=on-failure
RestartSec=10
KillMode=process

[Install]
WantedBy=multi-user.target


16.	In /var/homebridge, create configuration file config.json is shown below:


{
    "bridge": {
        "name": "Homebridge",
        "username": "CC:22:3D:E3:CE:31",
        "port": 51826,
        "pin": "031-45-154"
    },

    "description": "This is an example configuration file with all supported devices. You can use this as a template for creating your own configuration file containing devices you actually own.",

    "accessories": [
        {
            "accessory": "PiThermostat",
            "name": "House Thermostat",
            "ip_address": "localhost:9999",
            "username": "boss",
            "password": "boss",
            "http_method": "GET"
        }
    ]

}

If thermostat executable runs on a different Raspberry Pi, then the ip_address will be “ip_address:9999”, instead of “localhost:9999”.

17.	sudo systemctl daemon-reload

18.	sudo systemctl enable homebridge

19.	sudo systemctl start homebridge

