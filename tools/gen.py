import sys, time, datetime
def crc8(b):
    c = 0
    for x in b:
        c ^= x
        for _ in range(8):
            c = ((c << 1) ^ 0x07) & 0xFF if c & 0x80 else (c << 1) & 0xFF
    return c
files = int(sys.argv[1]); now = int(time.time()); wib = datetime.timezone(datetime.timedelta(hours=7)); n = 0
for f in range(files):
    with open(f"queue_{f}.csv", "w", newline="") as fh:
        fh.write("rfid,timestamp,device_id,unix_time,crc8\n")
        for _ in range(25):
            rfid = f"9{n:09d}"; t = now - 3600 - n
            ts = datetime.datetime.fromtimestamp(t, wib).strftime("%Y-%m-%d %H:%M:%S")
            crc = crc8(rfid.encode() + t.to_bytes(4, "big"))
            fh.write(f"{rfid},{ts},TEST,{t},{crc:02X}\r\n"); n += 1
