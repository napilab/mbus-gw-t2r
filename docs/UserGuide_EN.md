# mbusgw-t2r --- User Guide (English)

*A MODBUS TCP to RTU gateway. This guide is written for a beginner: every step is spelled out,
nothing is assumed. The Russian version of this guide is in `UserGuide_RU.md`.*

---

## 1. What this program does

You have a device (an electricity meter, a temperature controller, a PLC...) which talks
**MODBUS RTU** over a serial line (RS-485 or RS-232). And you have a program on your computer
or network (a SCADA, a script, `mbpoll`...) which talks **MODBUS TCP** over the network.

The gateway sits between them and translates:

```
 [Your TCP client] --- TCP/IP network ---> [mbusgw-t2r] --- serial line ---> [Your RTU device]
      (SCADA,                              (this program)   (RS-485/RS-232)   (meter, PLC, ...)
       mbpoll, ...)
```

- Several TCP clients may connect at the same time --- the gateway queues them so that only one
  request at a time goes to the serial line (an RTU line physically cannot do two at once).
- Several devices may sit on one RS-485 line --- they are told apart by the *slave address*
  inside the MODBUS request; the gateway does not need to know their addresses at all.

## 2. Before you start --- the checklist

1. The gateway is installed (see `README.md`, section *Installation*). After the installation
   you have:
   - the program itself: `/usr/local/sbin/mbusgw-t2r`
   - the settings file: `/usr/local/etc/mbusgw-t2r/modbus-t2r_settings.conf`
2. You know **which serial port** your device is connected to. On Linux it is a file like
   `/dev/ttyS0`, `/dev/ttyUSB0` or `/dev/ttyXRUSB0`. If unsure, plug the USB adapter out and
   in and run `dmesg | tail` --- the port name will be printed.
3. You know the **line parameters** of your device: speed (baud rate), data bits, parity and
   stop bits. They are in the device's manual. The most common set is `9600, 8, N, 1`.
4. Your user can open the port. The simplest check: `ls -l /dev/ttyUSB0` --- if the group is
   `dialout`, add yourself to it: `sudo usermod -a -G dialout $USER` (re-login afterwards),
   or simply run the gateway with `sudo`.

## 3. The settings file, line by line

The settings file has two sections: `serials` (the serial lines) and `listeners` (the TCP
ports). Open it in any editor:

```
$ sudo nano /usr/local/etc/mbusgw-t2r/modbus-t2r_settings.conf
```

A minimal working example:

```
serials = (
	{	device = "/dev/ttyUSB0";
		chars  = "9600, 8, N, 1";
		iotmo  = 1000;
	}
);

listeners = (
	{	bind   = "TCP:0.0.0.0:502";
		target = "/dev/ttyUSB0";
		connlm = 8;
	}
);
```

Read it as: *"open the port /dev/ttyUSB0 at 9600-8-N-1, wait for an answer up to 1000 ms;
listen for TCP clients on every network interface, port 502, and forward them to that port"*.

### 3.1. The `serials` section --- one record per serial port

| Key | Required? | What it means | Allowed values |
|-----|-----------|---------------|----------------|
| `device` | **yes** | The serial port file | e.g. `/dev/ttyUSB0` |
| `chars` | **yes** | The line parameters: `speed, data bits, parity, stop bits` | speed 50..4000000; data bits 5..8; parity `N` (none), `E` (even), `O` (odd); stop bits 1..2 |
| `iotmo` | no | How long to wait for the device's answer, in **milliseconds**. Default: 1000 | any positive number |
| `rs485` | no | `1` --- ask the kernel to drive the RS-485 direction control (only for ports that support it) | `0` or `1` |
| `ts_enabled` | no | `1` --- enable the built-in Time Stamp pseudo device (see section 6) | `0` or `1` |
| `ts_unit` | no | The slave address the pseudo device answers on | 1..247, default 135 |
| `ts_fncode` | no | The function code it answers to | default 4 |
| `ts_base_reg0` | no | The first register number of the time stamp | default 135 |

If a record is wrong, the gateway **skips it and says why** --- with the allowed range in the
message, for example:

```
%T2R-E: [serial #00:</dev/ttyUSB0>] --- speed 31 baud is out of range [50..4000000]
```

The record number (`#00`) counts from zero, in the order the records appear in the file.

### 3.2. The `listeners` section --- one record per TCP port

| Key | Required? | What it means | Allowed values |
|-----|-----------|---------------|----------------|
| `bind` | **yes** | Where to listen: `TCP:<IP address>:<port>` | port 1..65535; `0.0.0.0` = all interfaces; UDP is not supported |
| `target` | **yes** | Which serial port to forward to --- must match a `device` from `serials` **exactly** | |
| `connlm` | no | How many TCP clients may wait in the connection queue | small number, e.g. 8 |
| `iotmo` | no | The network I/O timeout, milliseconds | |

Notes for the very beginning:

- Port **502** is the standard MODBUS TCP port, but on Linux ports below 1024 need root.
  If you do not want to run as root, use e.g. `5502` and point your client there.
- Several listeners may share one `target` --- the gateway will serialize their requests.

## 4. Running the gateway

By hand, in a terminal (the messages go straight to the screen):

```
$ /usr/local/sbin/mbusgw-t2r /settings=/usr/local/etc/mbusgw-t2r/modbus-t2r_settings.conf
```

Useful options:

| Option | Meaning |
|--------|---------|
| `/settings=<file>` | The settings file (see above) |
| `/trace` | Verbose tracing: every request and answer is dumped in hex. Priceless for the first run |
| `/logfile=<file>` | Write the log to a file instead of the screen |
| `/logsize=<octets>` | Rotate the log file when it grows above this size |

A healthy start looks like this (shortened):

```
%T2R-I-REVISNF, Rev: T2R X.00-06/aarch64(built at ...) (REV: 00.06.00)
%T2R-I:  Added device #00 [</dev/ttyUSB0>, Chars: <9600, 8, N, 1>, ...] --- added
%T2R-I:  Added listener #00 [Target: </dev/ttyUSB0>, Net: <TCP:0.0.0.0:502>, ...] --- added
%T2R-S-DEVREADY, Device </dev/ttyUSB0> [9600 baud, answer tmo: 1000 msec, t3.5: 4010 usec] --- is ready
%T2R-S-LSNRRDY, [#3] Listener 0.0.0.0:502 [Target: </dev/ttyUSB0>] --- is ready
```

Two lines matter most: **DEVREADY** (the port is open) and **LSNRRDY** (the TCP port is
listening). If you see both --- the gateway is up.

To stop it: press `Ctrl+C` once and wait a second. To toggle the tracing of a *running*
gateway without a restart: `kill -USR1 <pid>`.

### 4.1. The first test with mbpoll

`mbpoll` is a small free MODBUS client (`sudo apt install mbpoll`). Read 3 holding registers
from the device with slave address 1:

```
$ mbpoll -a 1 -r 1 -c 3 -1 127.0.0.1 -p 502
```

If numbers come back --- everything works, end to end.

## 5. Reading the log

Every important event is printed as a line with a **message code**:

```
%T2R-E-CRC16ERR, [#4:</dev/ttyUSB0>] RTU CRC16 check error (received: 0x1234, calculated: 0xABCD)
 ^   ^  ^
 |   |  +-- the code: look it up in the table of section 7
 |   +----- severity: S=success, I=info, W=warning, E=error, F=fatal
 +--------- the facility (always T2R for this program)
```

`[#4:</dev/ttyUSB0>]` means: file descriptor #4, the serial port in question.

## 6. The Time Stamp pseudo device

The gateway can answer *by itself* --- without touching the line --- to a read request sent
to a special slave address. The answer is the current time of the gateway's host. This lets a
SCADA read precise time over plain MODBUS.

With `ts_enabled = 1; ts_unit = 135; ts_fncode = 4; ts_base_reg0 = 135;` a request "read N
input registers starting at 135 from slave 135" returns:

| Registers requested | You get |
|---------------------|---------|
| 4 | seconds of the UNIX epoch, 64 bits, the most significant 16-bit word first: R0=bits 63..48, R1=47..32, R2=31..16, R3=15..0 |
| 8 | + nanoseconds, packed the same way into R4..R7 |
| 9 | + R8 = the local timezone offset, in **signed minutes** (e.g. Moscow = +180) |

Assemble the seconds as `(R0<<48)|(R1<<32)|(R2<<16)|R3`.

Any other register count in a TS request is answered with the MODBUS exception
`ILLEGAL_DATA_ADDRESS` --- this is on purpose.

## 7. Troubleshooting --- symptom, cause, action

**The golden rule: run with `/trace` and read the log. The gateway always says what it
dislikes.**

| You see | What it means | What to do |
|---------|---------------|------------|
| `%T2R-E-DEVOPNERR, Cannot open the device </dev/ttyUSB0>, errno: 2` | errno 2 = the file does not exist: the adapter is unplugged or the name is wrong | `dmesg \| tail` after plugging the adapter; fix `device` in the settings |
| the same, `errno: 13` | Permission denied | run with `sudo` or add yourself to the `dialout` group |
| the same, `errno: 16` | The port is busy: another program holds it | find it: `sudo fuser /dev/ttyUSB0`; stop it |
| `%T2R-E-NOANSWER, [...] Did not get an answer in 1000 msec` | The request went out, nothing came back | 1) wrong slave address in the client's request; 2) wrong wiring (swap A/B of RS-485); 3) wrong speed/parity --- re-check `chars`; 4) the device is simply off |
| `%T2R-E-CRC16ERR, [...] (received: X, calculated: Y)` | Octets arrive, but damaged | 1) `chars` does not match the device (most common!); 2) line noise --- check the grounding, the termination resistors, the cable length; 3) two masters on one line |
| `%T2R-E-BADFRAME, [...] !UL octets, allowed [5..256]` | Something arrived, but it is not a sane MODBUS frame | usually the same causes as CRC16ERR; with 400+ octets of garbage --- a foreign, non-MODBUS device on the line |
| `%T2R-W-FRAMETMO, [...] Frame is not completed in ...` | The answer started but never finished in time | a very slow or hanging device; raise `iotmo`; check the cable |
| `%T2R-W-EXCRPT, [...] Report exception 4 (SERVER_DEVICE_FAILURE) ...` | The gateway told the TCP client "the serial side failed" | look at the *previous* log line --- it names the real reason (NOANSWER, CRC16ERR, ...) |
| `%T2R-E-LSNRERR, Listener 0.0.0.0:502 --- bind() error, errno: 98` | errno 98 = the TCP port is already taken | another gateway instance is running, or another program owns the port: `sudo ss -tlnp \| grep 502` |
| the same, `errno: 13` | Ports below 1024 need root | run with `sudo`, or use a port >= 1024 (e.g. 5502) |
| `%T2R-E: [serial #NN:...] --- ... out of range [a..b]` | A settings value is out of its allowed range | fix the value; the allowed range is right in the message |
| `%T2R-E: No serials has been defined!` | Not a single `serials` record survived the validation | read the error lines above it --- each rejected record says why |
| The client connects but every answer is an exception `0x04` | See EXCRPT above | the trouble is on the serial side; `/trace` and read backwards |
| Everything looks fine, but the data is wrong/shifted | Byte order confusion on the client side | MODBUS registers are big-endian 16-bit words; check how your client assembles 32/64-bit values |

### 7.1. The message codes reference

| Code | Severity | When it appears |
|------|----------|-----------------|
| `REVISNF` | I | At start: the program version. Quote it when asking for help |
| `DEVREADY` | S | The serial port is open and configured; shows the actual baud, the answer timeout and the t3.5 interval |
| `LSNRRDY` | S | The TCP port is listening; shows the address, the port and the target device |
| `NETCONN` | S | A TCP client has connected; shows its address:port and the listener |
| `NETDISCN` | S | A TCP client has disconnected; shows its address:port |
| `DEVOPNERR` | E | The serial port cannot be opened; `errno` says why (2 = no such file, 13 = permissions, 16 = busy) |
| `LSNRERR` | E | `bind()`/`listen()` failed for a TCP port; `errno` says why (98 = taken, 13 = privileges) |
| `NOANSWER` | E | The device did not answer within the `iotmo` timeout |
| `FRAMETMO` | W | The answer began but was not completed within the timeout |
| `BADFRAME` | E | The received frame length is outside the sane [5..256] range |
| `CRC16ERR` | E | The checksum of the received frame does not match; both values are shown |
| `EXCRPT` | W | A MODBUS exception is being returned to the TCP client; the code and its name are shown |
| `EXITST` | I | The gateway exits; shows the exit flag and the final status |

*(The `S/I/W/E` letter is a part of the printed line: `%T2R-E-CRC16ERR, ...`.)*

## 8. If nothing helps

Collect and attach to your question:

1. the full start-up log with `/trace` (from `REVISNF` to the first error),
2. your settings file,
3. the output of `ls -l <device>` and `dmesg | tail -20`,
4. the exact model of the RTU device and its documented line parameters.

With these four things the problem is almost always visible at a glance.
