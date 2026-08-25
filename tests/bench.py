#!/usr/bin/env python3
# Self-contained bench: pty pair + fake RTU slave + gateway + MBAP client
import os, pty, time, socket, struct, subprocess, threading, sys, signal

GW = sys.argv[1] if len(sys.argv) > 1 else './build/mbusgw-t2r'

def crc16(d):
    crc = 0xFFFF
    for b in d:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc

# --- pty pair via openpty (no socat needed) ---
m1, s1 = pty.openpty()          # gateway side: slave name of s1
import tty as ttymod
ttymod.setraw(m1); ttymod.setraw(s1)
gw_dev = os.ttyname(s1)
print("gateway tty:", gw_dev)

conf = f'''listeners = ( {{ bind = "TCP:127.0.0.1:5502"; connlm = 8; iotmo = 5000; target = "{gw_dev}"; }} );
serials = ( {{ device = "{gw_dev}"; chars = "2400, 8, N, 1"; iotmo = 2000; rs485 = 0;
	ts_enabled = 1; ts_unit = 135; ts_fncode = 4; ts_base_reg0 = 135; }} );
'''
open('/tmp/t2r_bench.conf','w').write(conf)

mode = {'v': 'frag'}
def slave():
    buf = b''
    while True:
        try:
            chunk = os.read(m1, 256)
        except OSError:
            return
        if not chunk: continue
        buf += chunk
        while len(buf) >= 8:
            req = buf[:8]; buf = buf[8:]
            slave_id, fn = req[0], req[1]
            qty = (req[4] << 8) | req[5]
            if mode['v'] == 'garbage':
                os.write(m1, os.urandom(400))
                continue
            vals = b''.join(int(1000+i).to_bytes(2,'big') for i in range(qty))
            pdu = bytes([slave_id, fn, qty*2]) + vals
            c = crc16(pdu)
            frame = pdu + bytes([c & 0xFF, c >> 8])
            if mode['v'] == 'frag':
                for b in frame:
                    os.write(m1, bytes([b])); time.sleep(0.005)
            else:
                os.write(m1, frame)

threading.Thread(target=slave, daemon=True).start()

gwp = subprocess.Popen([GW, '/trace', '/settings=/tmp/t2r_bench.conf'],
                       stdout=open('/tmp/gw.log','w'), stderr=subprocess.STDOUT)
time.sleep(1.0)
assert gwp.poll() is None, "gateway died: " + open('/tmp/gw.log').read()[-500:]

def mbap(txid, unit, pdu):
    return struct.pack('>HHHB', txid, 0, len(pdu)+1, unit) + pdu

ok = True
try:
    s = socket.create_connection(('127.0.0.1', 5502), timeout=10)

    # T1: fragmented slave answer, FC04 x3 (T2R-09)
    s.sendall(mbap(0x1234, 1, bytes([4, 0, 0, 0, 3])))
    r = s.recv(512)
    print("T1 frag resp:", r.hex())
    assert r[:2] == b'\x12\x34' and r[7] == 4 and r[8] == 6, "T1 header"
    vals = [int.from_bytes(r[9+i*2:11+i*2],'big') for i in range(3)]
    assert vals == [1000,1001,1002], f"T1 values {vals}"
    print("T1 PASS (fragmented RTU frame reassembled)")

    # T2: TS pseudo-device 4 regs (T2R-05 packing, T2R-06 length)
    s.sendall(mbap(2, 135, bytes([4, 0x00, 0x87, 0, 4])))
    r = s.recv(512)
    print("T2 TS resp:", r.hex())
    mlen = int.from_bytes(r[4:6],'big')
    assert mlen == 11, f"T2 MBAP.len {mlen} != 11 (T2R-06)"
    assert len(r) == 6 + mlen, "T2 declared/actual mismatch"
    regs = [int.from_bytes(r[9+i*2:11+i*2],'big') for i in range(4)]
    ts = (regs[0]<<48)|(regs[1]<<32)|(regs[2]<<16)|regs[3]
    print("T2 regs:", [hex(x) for x in regs], "ts:", ts, "now:", int(time.time()))
    assert abs(ts - time.time()) < 10, "T2 packing (T2R-05)"
    print("T2 PASS (MBAP.len=11, 64->4x16 packing correct)")

    # T3: TS 9 regs, tz offset in signed minutes
    s.sendall(mbap(3, 135, bytes([4, 0x00, 0x87, 0, 9])))
    r = s.recv(512)
    mlen = int.from_bytes(r[4:6],'big')
    assert mlen == 21 and len(r) == 27, f"T3 len {mlen}/{len(r)}"
    off = int.from_bytes(r[9+16:11+16],'big')
    off = off - 65536 if off > 32767 else off
    print("T3 PASS (MBAP.len=21, tz offset:", off, "min)")

    # T4: 400 octets of line garbage (T2R-02 stack safety)
    mode['v'] = 'garbage'
    s.sendall(mbap(4, 1, bytes([4, 0, 0, 0, 2])))
    r = s.recv(512)
    print("T4 garbage resp:", r.hex())
    assert r[7] & 0x80, "T4 expected an exception response"
    print("T4 PASS (garbage rejected with MODBUS exception, no crash)")

    # T5: gateway still alive and serves a normal request after the garbage
    mode['v'] = 'whole'
    s.sendall(mbap(5, 1, bytes([4, 0, 0, 0, 2])))
    r = s.recv(512)
    vals = [int.from_bytes(r[9+i*2:11+i*2],'big') for i in range(2)]
    assert vals == [1000,1001], "T5 recovery"
    print("T5 PASS (recovered after garbage)")

    s.close()

    # T6: thread hygiene - 60 short sessions, thread count must not grow (T2R-07)
    for i in range(60):
        c = socket.create_connection(('127.0.0.1', 5502), timeout=5); c.close()
    time.sleep(1.0)
    nthreads = len(os.listdir(f'/proc/{gwp.pid}/task'))
    print("T6 threads after 60 sessions:", nthreads)
    assert nthreads < 10, "T6 thread leak (T2R-07)"
    print("T6 PASS (no thread leak)")

    print("ALL PASS")
except Exception as e:
    ok = False
    print("FAIL:", e)
    print("--- gw.log tail ---")
    print(open('/tmp/gw.log').read()[-2000:])
finally:
    gwp.send_signal(signal.SIGTERM); time.sleep(0.3)
    gwp.send_signal(signal.SIGTERM); time.sleep(0.5)
    if gwp.poll() is None: gwp.kill()

sys.exit(0 if ok else 1)
