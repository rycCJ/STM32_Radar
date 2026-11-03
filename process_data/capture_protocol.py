"""Incremental RDR1 / unchanged legacy stream decoder. No serial/numpy dependency."""
from dataclasses import dataclass
import struct
import zlib

HEADER = struct.Struct('<4sBBHIIQIHHHBBIfI')
MAGIC = b'RDR1'
MAX_PAYLOAD = 65536
KINDS = {1: 'ADC', 2: 'IQ', 3: 'POINT'}


@dataclass
class Frame:
    mode: str
    payload: bytes
    sequence: int | None = None
    ticks: int | None = None
    tick_hz: int | None = None
    count: int | None = None
    chirp: int | None = None
    slot: int | None = None
    sample_type: int | None = None
    flags: int = 0
    adc_hz: int | None = None
    bin_m: float | None = None

    def metadata(self):
        return {k: v for k, v in vars(self).items() if k != 'payload'}

    def values(self):
        """Return raw samples, IQ planes, or encoded point packet; no algorithms."""
        if self.mode in ('ADC', 'ADC_LEGACY'):
            return struct.unpack('<' + 'h' * (len(self.payload)//2), self.payload)
        if self.mode == 'IQ':
            n = self.count
            values = struct.unpack('<' + 'f' * (2*n), self.payload)
            return values[:n], values[n:]
        if self.mode == 'LEGACY':
            values = struct.unpack('<300f', self.payload[2:1202])
            return values[:150], values[150:], self.payload[1204:]
        return self.payload


class Decoder:
    def __init__(self, format='new'):
        if format not in ('new', 'legacy', 'adc-legacy'):
            raise ValueError('Unknown format')
        self.format = format
        self.buffer = bytearray()
        self.stats = dict(frames=0, crc_errors=0, malformed=0, discarded_bytes=0,
                          missing_frames=0, resets=0, sequence_errors=0)
        self.last_sequence = self.last_ticks = None

    def _discard(self, n=1):
        del self.buffer[:n]
        self.stats['discarded_bytes'] += n

    def feed(self, data):
        self.buffer.extend(data)
        frames = []
        magic = {'new': MAGIC, 'legacy': b'\x66\xbb',
                 'adc-legacy': bytes(range(1, 9))}[self.format]
        while True:
            at = self.buffer.find(magic)
            if at < 0:
                # Retain only a possible partial magic suffix.
                keep = 0
                for n in range(1, min(len(magic), len(self.buffer)+1)):
                    if self.buffer[-n:] == magic[:n]: keep = n
                self._discard(len(self.buffer)-keep)
                break
            if at: self._discard(at)
            if self.format == 'adc-legacy':
                if len(self.buffer) < 1032: break
                frame = Frame('ADC_LEGACY', bytes(self.buffer[8:1032]), count=512)
                total = 1032
            elif self.format == 'legacy':
                if len(self.buffer) < 1216: break
                cloud = self.buffer[1204:]
                length = struct.unpack_from('<I', cloud, 2)[0]
                count = struct.unpack_from('<H', cloud, 10)[0]
                if (self.buffer[1202:1204] != b'\xbb\x66' or cloud[:2] != b'\x55\xaa'
                        or cloud[8:10] != b'\x01\x01' or length != 6+9*count
                        or length > MAX_PAYLOAD):
                    self.stats['malformed'] += 1; self._discard(); continue
                total = 1210+length
                if len(self.buffer) < total: break
                frame = Frame('LEGACY', bytes(self.buffer[:total]), count=count)
            else:
                if len(self.buffer) < HEADER.size: break
                (_, version, kind, hlen, size, seq, ticks, hz, count, chirp,
                 slot, sample, flags, adc_hz, bin_m, reserved) = HEADER.unpack_from(self.buffer)
                valid = (version == 1 and hlen == 48 and kind in KINDS
                         and size <= MAX_PAYLOAD and hz > 0 and reserved == 0
                         and ((kind == 1 and sample == 1 and size == count*2 and count == 512)
                              or (kind == 2 and sample == 2 and size == count*8 and count == 150)
                              or (kind == 3 and sample == 3 and size == 12+count*9)))
                if not valid:
                    self.stats['malformed'] += 1; self._discard(); continue
                total = hlen+size+4
                if len(self.buffer) < total: break
                expected = struct.unpack_from('<I', self.buffer, hlen+size)[0]
                if zlib.crc32(self.buffer[4:hlen+size]) != expected:
                    self.stats['crc_errors'] += 1; self._discard(); continue
                payload = bytes(self.buffer[hlen:hlen+size])
                if kind == 3 and (payload[:2] != b'\x55\xaa'
                     or struct.unpack_from('<I', payload, 2)[0] != size-6
                     or payload[8:10] != b'\x01\x01'
                     or struct.unpack_from('<H', payload, 10)[0] != count):
                    self.stats['malformed'] += 1; self._discard(); continue
                frame = Frame(KINDS[kind], payload, seq, ticks, hz, count,
                              chirp, slot, sample, flags, adc_hz, bin_m)
                if self.last_sequence is not None:
                    if (flags & 1) or ticks < self.last_ticks:
                        self.stats['resets'] += 1
                    else:
                        delta = (seq-self.last_sequence) & 0xffffffff
                        if 1 < delta < 0x80000000: self.stats['missing_frames'] += delta-1
                        elif delta != 1: self.stats['sequence_errors'] += 1
                self.last_sequence, self.last_ticks = seq, ticks
            del self.buffer[:total]
            self.stats['frames'] += 1
            frames.append(frame)
        return frames

    def finish(self):
        return dict(self.stats, trailing_bytes=len(self.buffer))
