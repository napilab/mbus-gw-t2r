## General memories


### Get MODBUS requester software and do a small &quick checks
```
git clone https://github.com/sourceperl/mbtget.git
cd mbtget
perl Makefile.PL
make
sudo make install
```


```
mbtget -a 1 127.0.0.1 -d -r4 -n 2
Tx
[BA C5 00 00 00 06 01] 04 00 01 00 02

Rx
[BA C5 00 00 00 09 01] 04 04 00 F4 01 67 FA

values:
 1 (ad 00001):   244
 2 (ad 00002):   359
```


### MODBUS Gateway side log
```
13:12:47: Debugging /sysman/Works/DOK/mbusd-by-SysMan/build/mbusgw-t2r /config=modbus-t2r.conf /settings=modbus-t2r_settings.conf ...
15-10-2025 13:12:47.288  62674 [T2R-MAIN\main:479] %T2R-I:  Rev: X.00-01/x86_64, (built  at Oct 15 2025 13:12:46 with CC 12.2.0)
15-10-2025 13:12:47.288  62674 [UTIL$\__util$showparams:670] %UTILS-I:  trace = ON
15-10-2025 13:12:47.288  62674 [UTIL$\__util$showparams:693] %UTILS-I:  logfile[0:0] =''
15-10-2025 13:12:47.288  62674 [UTIL$\__util$showparams:687] %UTILS-I:  logsize = 0 (0)
15-10-2025 13:12:47.288  62674 [UTIL$\__util$showparams:693] %UTILS-I:  settings[0:24] ='modbus-t2r_settings.conf'
15-10-2025 13:12:47.288  62674 [T2R-MAIN\s_settings_process_serials:257] %T2R-I:  Added device #00 [</dev/ttyUSB0>, Chars: <9600, 8, N, 1>, Answer tmo: 5000 msec, RTU PDU tmo: 1 msec] --- added
15-10-2025 13:12:47.288  62674 [T2R-MAIN\s_settings_process_listeners:344] %T2R-I:  Added listener #00 [Target: </dev/ttyUSB0>, Net: <TCP:0.0.0.0:502>, I/O Tmo: 5000 msec, Connection limit: 8] --- added
15-10-2025 13:12:47.288  62674 [T2R-MAIN\s_settings_find_n_get_serial:155] %T2R-E:  Target device </dev/ttyS1> has not been defined
15-10-2025 13:12:47.288  62674 [T2R-MAIN\s_settings_process_listeners:334] %T2R-E:  [listener #01] --- target </dev/ttyS1> has not been define in <serial> section
15-10-2025 13:12:47.288  62674 [T2R-MAIN\s_settings_find_n_get_serial:155] %T2R-E:  Target device </dev/ttyS0> has not been defined
15-10-2025 13:12:47.288  62674 [T2R-MAIN\s_settings_process_listeners:334] %T2R-E:  [listener #02] --- target </dev/ttyS0> has not been define in <serial> section
15-10-2025 13:12:47.288  62674 [T2R-MAIN\s_init_sig_handler:450]        Set handler for signal 15/0xf (Terminated)
15-10-2025 13:12:47.288  62674 [T2R-MAIN\s_init_sig_handler:450]        Set handler for signal 2/0x2 (Interrupt)
15-10-2025 13:12:47.288  62674 [T2R-MAIN\s_init_sig_handler:450]        Set handler for signal 10/0xa (User defined signal 1)
15-10-2025 13:12:47.288  62674 [T2R-MAIN\s_init_sig_handler:450]        Set handler for signal 3/0x3 (Quit)
15-10-2025 13:12:47.288  62674 [T2R-TTY\s_tty_lock:76]                  Got exclusive access to </dev/ttyUSB0> ...
15-10-2025 13:12:47.501  62674 [T2R-TTY\s_tty_open:573]                 Trying to enable RS-485 support for /dev/ttyUSB0
15-10-2025 13:12:47.501  62674 [T2R-TTY\s_tty_open:578] %T2R-W:  ioctl(</dev/ttyUSB0>, TIOCGRS485)->-1, errno: 25 --- disabled RS485 support
15-10-2025 13:12:47.501  62674 [T2R-TTY\s_tty_unlock:95]                Release lock for </dev/ttyUSB0> (errno: 0)
15-10-2025 13:12:47.501  62674 [T2R-NET\t2r$net_start_listeners:484] %T2R-S:  [#3] Listener [0.0.0.0:502, Target: <#4:/dev/ttyUSB0>] --- initialized
15-10-2025 13:13:00.658  62677 [T2R-NET\s_net_listener:384]             [#3] Accept connection from 127.0.0.1:38056 on SD: #5
15-10-2025 13:13:00.658  62681 [T2R-NET\s_net_session:232]              [#5] Serves session for 127.0.0.1:38056 ....
15-10-2025 13:13:00.658  62681 [T2R-MODBUS\t2r$mbap_print:138] %T2R-I:  MBAP Header [TXID: 47813(0xbac5), PROTO: 0(0), LEN: 6(0x6) octets, UNIT: 1, FUN: 4(0x4-READ_INPUT_REGISTERS)]
15-10-2025 13:13:00.658  62681 [s_net_session:288] Dump of 12 octets follows:
	+0000:  ba c5 00 00 00 06 01 04 00 01 00 02               ............        
15-10-2025 13:13:00.658  62681 [T2R-TTY\s_tty_lock:76]                  Got exclusive access to </dev/ttyUSB0> ...
15-10-2025 13:13:01.043  62681 [T2R-TTY\s_tty_rtu_rx:203]               Start reading PDU ...
15-10-2025 13:13:01.051  62681 [T2R-TTY\s_tty_rtu_rx:237]               Stop reading PDU ...
15-10-2025 13:13:01.051  62681 [T2R-MODBUS\t2r$rtu_print:166] %T2R-I:  RTU PDU [LEN: 9(0x9) octets, SLAVE: 1, FUN: 4(0x4-READ_INPUT_REGISTERS), CRC: 0xcfa(calculated: 0xcfa)]
15-10-2025 13:13:01.051  62681 [T2R-TTY\s_tty_unlock:95]                Release lock for </dev/ttyUSB0> (errno: 0)
15-10-2025 13:13:01.051  62681 [T2R-MODBUS\t2r$mbap_print:138] %T2R-I:  MBAP Header [TXID: 47813(0xbac5), PROTO: 0(0), LEN: 9(0x9) octets, UNIT: 1, FUN: 4(0x4-READ_INPUT_REGISTERS)]
15-10-2025 13:13:01.051  62681 [s_net_session:304] Dump of 14 octets follows:
	+0000:  ba c5 00 00 00 09 01 04 04 00 f4 01 67 fa         ............g.      
15-10-2025 13:13:01.051  62681 [T2R-NET\s_net_recvn:59] %T2R-W:  [#5] --- peer close connection, errno: 0
15-10-2025 13:13:01.051  62681 [T2R-NET\s_net_session:344]              [#5] Close session for 127.0.0.1:38056 ....
```





### Using mbpool

```
mbpoll -1 -a 1 -t 3 -r 2 127.0.0.1 -c 1 -v
debug enabled
Set device=127.0.0.1
mbpoll 1.0-0 - FieldTalk(tm) Modbus(R) Master Simulator
Copyright © 2015-2019 Pascal JEAN, https://github.com/epsilonrt/mbpoll
This program comes with ABSOLUTELY NO WARRANTY.
This is free software, and you are welcome to redistribute it
under certain conditions; type 'mbpoll -w' for details.

Connecting to [127.0.0.1]:502
Set response timeout to 1 sec, 0 us
Protocol configuration: Modbus TCP
Slave configuration...: address = [1]
			start reference = 2, count = 1
Communication.........: 127.0.0.1, port 502, t/o 1.00 s, poll rate 1000 ms
Data type.............: 16-bit register, input register table

-- Polling slave 1...
[00][01][00][00][00][06][01][04][00][01][00][01]
Waiting for a confirmation...
<00><01><00><00><00><07><01><04><02><00><F3>
[2]:    243

root@devuan4-sysman:/sysman/Works/cc2531-sniffer/build#
```


### Memories for Lab

```
FCC-3308 AMVDK15:
modpoll -0 -b 9600 -p none -m rtu -a 1 -t 1 -r 1  /dev/ttyUSB0
FCC-3308 GT-TH-01:
modpoll -0 -b 9600 -p none -m rtu -a 9  -r 1 /dev/ttyUSB0
FCC-3308 isp-con isn-101:
modpoll -0 -b 9600 -p none -m rtu  -a 96  -t 3  -r 5 /dev/ttyS1
FCC-3308:
modpoll -0 -b 9600 -p none -m rtu -a 2 -t 3 -r 514 /dev/ttyS1 #PM-3112-160 джамперы 1000000110


modpoll -0 -m tcp -a 1 -t 1 -r 1 -p 502	 127.0.0.1	#/dev/ttyUSB0 #TCP:0.0.0.0:502
modpoll -0 -m tcp -a 9      -r 1 -p 502	 127.0.0.1	#/dev/ttyUSB0 #TCP:0.0.0.0:502
modpoll -0 -p 503 -m tcp  -a 96  -t 3  -r 5 127.0.0.1 #/dev/ttyS1
modpoll -0 -m tcp -a 2   -t 3  -r 514  -p 503 127.0.0.1 #/dev/ttyS1
```



### Request TS ...
```
root@devuan4-sysman:/sysman/Works/DOK/mbusd-by-SysMan# mbpoll -1 -a 1 -t 3:hex -r 136 127.0.0.1 -c 4 -v

Connecting to [127.0.0.1]:502
Set response timeout to 1 sec, 0 us
Protocol configuration: Modbus TCP
Slave configuration...: address = [1]
			start reference = 136, count = 4
Communication.........: 127.0.0.1, port 502, t/o 1.00 s, poll rate 1000 ms
Data type.............: 16-bit register, input register table

-- Polling slave 1...
[00][01][00][00][00][06][01][04][00][87][00][04]
Waiting for a confirmation...
<00><01><00><00><00><0D><01><04><08><00><69><69><0B><0B><7C><7C><18>
[136]:  0x0069
[137]:  0x690B
[138]:  0x0B7C
[139]:  0x7C18
```



## Links & References
```
https://www.kernel.org/doc/html/latest/driver-api/serial/serial-rs485.html
```

