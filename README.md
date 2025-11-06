# MODBUS TCP to RTU gateway
A yet another gateway for MODBUS TCP to RTU (StarLet Team Edition)

###	Introduction

A simple TCP-to-RTU gateway for MODBUS protocol is supposed to be used as a tutorial for:

 - a development of programming skills
 - a programming for I/O on  RS323 and RS-485 ports
 - basic checking of work of MODBUS-capable devices
 - build two-directionional gateways for TCP-to-RTU and RTU-to-TCP
 



### 	Build from sources

```
$ git clone  https://gitlab.com/SysMan-One/mbus-gw-t2r
$ cd mbus-gw-t2r
$ git submodule update --init

$ mkdir build
$ cd build
$ cmake ../CMakeLists.txt -B ./
```
 or

```
$ cmake ../CMakeLists.txt -DCMAKE_BUILD_TYPE=Debug -B ./
$ make -s
```

### 	Quick configuration

A configuration option is a single line text string  starting with "-" or "/",  option case is not matter :

`/<option_name>=<option_value>`

or 

`-<option_name>=<option_value>`

Example:


-logfile=/tmp/ttr.log

| Option | Format        | Fields  | Description                                                  |
| ------ | ------------- | ------- | ------------------------------------------------------------ |




## Authors and acknowledgment

Developer - Ruslan (AKA : The BadAss SysMan) Laishev, VAX/VMS bigot, BMF.
