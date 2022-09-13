#
# Simple sanitizer for eliminating some information from telemetry payloads
#

import struct

telesamples_sanitize = open('tests/fixtures/telemetry-samples.txt', 'a')

telesamples = open('tests/fixtures/telemetry-samples.orig', 'r')
for line in telesamples.readlines():
    line = line.strip()
    payload = bytearray(bytes.fromhex(line))
    bogus_id = struct.pack('>h', 1337)
    payload[3] = bogus_id[0]
    payload[4] = bogus_id[1]
    telesamples_sanitize.write(bytes(payload).hex() + '\r\n')

telesamples.close()
telesamples_sanitize.close()
