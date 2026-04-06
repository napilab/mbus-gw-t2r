# MODBUS TCP to RTU gateway
A yet another gateway for MODBUS TCP to RTU (Limited Edition especialy for LAB240 and NAPILab Teams)

###	NAPI World
This project is developed and maintained by the NAPI Lab team
and is primarily tested on NAPI industrial single-board computers based on Rockchip SoCs.

####	NAPI Boards

If you are looking for a reliable hardware platform to run MODBUS transport sowftware in production,
check out the NAPI board lineup:

Welcome to NAPI Wolrd (https://github.com/napilab/napi-boards) for more information!

Right now is available:

- NAPI2 — RK3568J, RS-485 onboard, Armbian
- NAPI-C — RK3308, compact, industrial grade


###	Introduction

A easy-to-use TCP-to-RTU gateway for MODBUS protocol is supposed to be used as a tutorial for:

 - a development of general programming skills
 - a programming skill esspecialy for I/O on  RS323 and RS-485 ports
 - basic checking of work of MODBUS-capable devices
 - build two-directionional gateways for TCP-to-RTU and RTU-to-TCP (not implemented yet)
 - transparently pass serial I/O over TCP

###	Main features & advantages
  - support of RS-232 and RS-485 specific signaling
  - single process with multithreaded architecture for high performance
  - coordinating access between multiple TCP-clients is connected to single serial bus\device
  - robust error handling and recognizing of serial-line garbage and trouble-recovery tecnicks
    



### 	Build from sources

```
$ git clone  <URL of repo>
$ cd mbus-gw-t2r
$ git submodule update --init --recursive

$ mkdir build
$ cd build
$ cmake ../CMakeLists.txt -B ./
```
 or

```
$ cmake ../CMakeLists.txt -DCMAKE_BUILD_TYPE=Debug -B ./
$ make -s
```

### 	Run & Configuration rules

####	Quick start

##### General CLI format to start gateway
```
./build/mbusgw-t2r <CLI options> --settings=<network-n-serials-settings.fil>
```

##### Example:

```
./build/mbusgw-t2r --trace --settings=modbus-t2r-settins.conf
```

##### CLI options

| Option		|  Description
| ------		| ------------------------------------------------------------
| trace			| Enable extensible diagnostic output. Useful for for debug and troubleshouting purpose.
| logfile=\<fpsec\>	| Set a file name to accept logging output
| logsize=\<number\>	| Limit size of log file.
| settings=\<fspec\>	| Provide a rin-time configuration for network stuff and serial devices


##### Settings options
Check an example of settings file for reference of parameters and rules of configurations

## Authors and acknowledgment
Developer: Ruslan (AKA : The BadAss SysMan) Laishev
VAX/VMS bigot,
BMF.
