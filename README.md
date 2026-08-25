# MODBUS TCP to RTU gateway
A yet another gateway for MODBUS TCP to RTU (Limited Edition especialy for LAB240 and NaPiLab Teams)

###	NaPi World
This project is developed and maintained by the NaPi Lab team
and is primarily tested on NaPi industrial single-board computers based on Rockchip SoCs.

####	NaPi Boards
If you are looking for a reliable hardware platform to run MODBUS transport sowftware in production,
check out the NaPi board lineup:

Welcome to NaPi Wolrd (https://github.com/napilab/napi-boards) for more information!

Right now is available:

- NAPI2 — RK3568J, RS-485 onboard, Armbian
- NAPI-C — RK3308, compact, industrial grade


###	Introduction
A easy-to-use TCP-to-RTU gateway for MODBUS protocol is supposed to be used as a tutorial (and not only) for:

 - a development of general programming skills
 - a programming skill esspecialy for I/O on  RS323 and RS-485 ports
 - basic checking of work of MODBUS-capable devices
 - build two-directionional gateways for TCP-to-RTU and RTU-to-TCP (not implemented yet)
 - transparently pass serial I/O over TCP

###	Main features & advantages
  - serves multiple MODBUS-capable devices on single RS-485 line
  - serves single RS-485 line on multiple TCP-ports
  - support of RS-232 and RS-485 specific signaling
  - single process with multithreaded architecture for high performance
  - coordinating access between multiple TCP-clients is connected to single serial bus\device
  - robust error handling and recognizing of serial-line garbage and trouble-recovery tecnicks

###	Reliability, footprint & performance

**No error slips through.** Every RTU frame coming from the line is validated before it is
believed: the CRC16 trailer is verified, the frame length is checked against the sane
[5..256] octets range, the end-of-frame is detected by the honest t3.5 silence criteria of
the MODBUS over Serial Line specification, and the answer is matched against the request by
the slave address and the function code. Wire-declared lengths are never trusted. Line
garbage --- even hundreds of octets of it --- is rejected with a clean MODBUS exception to
the TCP client, the line is flushed, and the very next request is served normally.

**No error is silent.** Every rejection produces a coded log message (`%T2R-E-CRC16ERR`,
`%T2R-E-NOANSWER`, ...) which names the reason: the received and the calculated CRC values,
the `errno` of a failed system call, the allowed range of a misconfigured parameter. The log
is designed to be grepped by the code and to make the troubleshooting a reading exercise,
not a guessing one --- see the User Guide, Chapter 7.

**Small footprint.** A single process; the only runtime dependency beyond libc is libconfig.
The binary is about 100 KB; the StarLet set is linked statically. There are no busy loops
anywhere: all the waiting is `poll()`-driven, so an idle gateway consumes practically zero
CPU. One thread exists per *active* TCP session only and is released as soon as the client
disconnects (no thread or memory leaks --- verified by the bundled test bench and by the
ASan/UBSan builds). This makes the gateway comfortable on the small industrial single-board
computers of the NaPi lineup (RK3308, RK3568J) --- and on anything larger.

**The line runs at its physical maximum.** The end-to-end latency is dominated by the serial
line itself; the TCP side adds microseconds (`TCP_NODELAY` is set, the answers are written
in one piece). Concurrent TCP clients are serialized per serial line by a mutex, so the RTU
bus is utilized back-to-back --- with the specification-mandated t3.5 inter-frame gap and
nothing more --- while every client still gets its answer in the order of arrival. Multiple
independent serial lines are served in parallel, each at its own full speed.
    



### 	Installation

The gateway depends on two packages: **libconfig** (a settings file parser, is taken from the
distro) and **StarLet** (the UTILITY$ROUTINES set, is installed from sources as a CMake package).
The whole sequence from a bare system to a running gateway is:

####	Step 1. Install the build prerequisites

```
$ sudo apt install build-essential cmake pkg-config libconfig-dev
```
(on RPM-based distros: `sudo dnf install gcc make cmake pkgconf libconfig-devel`)

####	Step 2. Install the StarLet package

```
$ git clone https://gitlab.com/SysMan-One/utility_routines
$ cd utility_routines
$ mkdir build && cd build
$ cmake ../ -DCMAKE_BUILD_TYPE=Release
$ make -s
$ sudo cmake --install .
```

The package is put under `/usr/local`: the `libstarlet` library, the public headers and the
CMake package configuration (`find_package(StarLet)` becomes available). To install to another
prefix use `sudo cmake --install . --prefix /opt/starlet` and then pass
`-DCMAKE_PREFIX_PATH=/opt/starlet` to the gateway's cmake at the Step 3.

####	Step 3. Build the gateway

```
$ git clone <URL of repo>
$ cd mbus-gw-t2r
$ mkdir build && cd build
$ cmake ../ -DCMAKE_BUILD_TYPE=Release
$ make -s
```

Build types: `Release` (production), `Debug` (symbols, tracing), `Asan` (address/UB sanitizers
for the development).

####	Step 4. Install the gateway

```
$ sudo make install
```

What goes where:

| What | Where |
|------|-------|
| `mbusgw-t2r` binary | `/usr/local/sbin/` |
| Configuration files | `/usr/local/etc/mbusgw-t2r/` |
| Docs and reference copies of the configs | `/usr/local/share/doc/mbusgw-t2r/` |

The configuration files are installed **only when they are absent**: a repeated `make install`
(an upgrade) never overwrites the has been tuned working configuration - such files are reported
as `Keeping the existing ...`.

####	Step 5. Tune the configuration and run

```
$ sudo vi /usr/local/etc/mbusgw-t2r/modbus-t2r_settings.conf
$ /usr/local/sbin/mbusgw-t2r /settings=/usr/local/etc/mbusgw-t2r/modbus-t2r_settings.conf
```

See the *Run & Configuration rules* section below for the settings file format.

####	Uninstallation

```
$ cd mbus-gw-t2r/build
$ sudo make uninstall
```

Everything which has been put by `make install` is removed, except the configuration files
under `etc/mbusgw-t2r/` - the has been tuned working configuration always survives.

### 	User Guide

A detailed, beginner-level guide --- configuration walk-through, running, the Time Stamp
pseudo device, troubleshooting by the message codes --- is available in two languages:

- English: [docs/UserGuide_EN.md](docs/UserGuide_EN.md) (a printable DEC-styled PDF: [docs/UserGuide_EN.pdf](docs/UserGuide_EN.pdf))
- Russian: [docs/UserGuide_RU.md](docs/UserGuide_RU.md) (a printable DEC-styled PDF: [docs/UserGuide_RU.pdf](docs/UserGuide_RU.pdf))

The PDFs are generated by `docs/make_userguide_pdf.py` (the reportlab set, the DejaVu fonts).

After `make install` both are also in `/usr/local/share/doc/mbusgw-t2r/`.

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
