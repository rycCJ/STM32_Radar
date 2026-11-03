import json
from pathlib import Path
import random
import struct
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch
import zlib

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'process_data'))
from capture_protocol import Decoder, HEADER
from capture_serial import capture, expect, sync


def cloud(count=1):
    return b'\x55\xaa'+struct.pack('<I H B B H',6+9*count,100,1,1,count)+bytes(9*count)


def packet(kind=2,seq=0,ticks=3200,flags=0):
    payload={1:struct.pack('<512h',*range(512)),2:struct.pack('<300f',*range(300)),3:cloud()}[kind]
    count={1:512,2:150,3:1}[kind]
    header=HEADER.pack(b'RDR1',1,kind,48,len(payload),seq,ticks,32000,count,
                       2 if kind==2 else 0,6 if kind==2 else 0,kind,flags,10000000,.050041,0)
    return header+payload+struct.pack('<I',zlib.crc32(header[4:]+payload))


class ProtocolTests(unittest.TestCase):
    def test_random_chunking_and_values(self):
        wire=b''.join(packet(k,i,3200+i*3200) for i,k in enumerate((1,2,3)))
        decoder=Decoder(); frames=[]; rng=random.Random(17)
        while wire:
            n=rng.randrange(1,160); frames+=decoder.feed(wire[:n]); wire=wire[n:]
        self.assertEqual([f.mode for f in frames],['ADC','IQ','POINT'])
        self.assertEqual(frames[0].values()[-1],511)
        self.assertEqual(frames[1].values()[1][-1],299)
        self.assertEqual(decoder.finish()['trailing_bytes'],0)

    def test_crc_corruption_resynchronizes(self):
        bad=bytearray(packet()); bad[100]^=1
        d=Decoder(); result=d.feed(b'boot log\n'+bad+packet(seq=1))
        self.assertEqual(len(result),1); self.assertEqual(d.stats['crc_errors'],1)

    def test_malformed_header_does_not_allocate_unbounded(self):
        bad=bytearray(packet()); struct.pack_into('<I',bad,8,0xffffffff)
        d=Decoder(); result=d.feed(bad+packet(seq=2))
        self.assertEqual(len(result),1); self.assertGreater(d.stats['malformed'],0)

    def test_truncated_record(self):
        d=Decoder(); self.assertEqual(d.feed(packet()[:-3]),[])
        self.assertGreater(d.finish()['trailing_bytes'],0)
        self.assertEqual(len(d.feed(packet(seq=1))),1)

    def test_sequence_gap_reset_duplicate_and_wrap(self):
        d=Decoder(); d.feed(packet(seq=10)+packet(seq=13,ticks=6400))
        self.assertEqual(d.stats['missing_frames'],2)
        d.feed(packet(seq=0,ticks=20,flags=1))
        self.assertEqual(d.stats['resets'],1)
        d.feed(packet(seq=0,ticks=30))
        self.assertEqual(d.stats['sequence_errors'],1)
        d=Decoder(); d.feed(packet(seq=0xffffffff)+packet(seq=0,ticks=6400))
        self.assertEqual(d.stats['missing_frames'],0)
        self.assertEqual(d.stats['sequence_errors'],0)

    def test_partial_magic_and_noise(self):
        d=Decoder(); self.assertEqual(d.feed(b'noiseRD'),[])
        self.assertEqual(len(d.feed(packet()[2:])),1)

    def test_legacy_and_adc_legacy(self):
        iq=b'\x66\xbb'+struct.pack('<300f',*range(300))+b'\xbb\x66'
        d=Decoder('legacy'); out=d.feed(iq+cloud(0)+iq+cloud(2))
        self.assertEqual([f.count for f in out],[0,2])
        self.assertEqual(out[0].values()[0][149],149)
        d=Decoder('adc-legacy'); out=d.feed(bytes(range(1,9))+struct.pack('<512h',*range(512)))
        self.assertEqual(out[0].values()[-1],511)

    def test_real_iq_record_when_available(self):
        records=list((ROOT/'process_data/1d_data').glob('Serial Debug 2026-10-30 200503.txt'))
        if not records: self.skipTest('local recording is intentionally not committed')
        raw=bytes.fromhex(records[0].read_text(encoding='utf-8-sig'))
        d=Decoder('legacy'); frames=d.feed(raw)
        self.assertEqual(len(frames),57)
        self.assertEqual(d.stats['malformed'],0)
        self.assertEqual(d.finish()['trailing_bytes'],0)


class FakeSerial:
    def __init__(self,*args,**kwargs):
        self.rx=bytearray(); self.started=False; self.closed=False; self.commands=[]
    @property
    def in_waiting(self):
        if self.started and not self.rx:
            self.rx.extend(packet(flags=1)); self.started=False
        return len(self.rx)
    def reset_input_buffer(self): self.rx.clear()
    def write(self,data):
        self.commands.append(data)
        cmd=data.strip()
        if cmd==b'AT': self.started=False; self.rx.extend(b'AT+OK\n')
        elif cmd.startswith(b'AT+MODE=') or cmd.startswith(b'AT+LINK='): self.rx.extend(cmd+b'\n')
        elif cmd==b'AT+START': self.rx.extend(b'AT+STARTED\n'); self.started=True
        elif cmd==b'AT+STATUS?': self.rx.extend(b'AT+STATUS=0,0,0\n')
        return len(data)
    def read(self,n=1):
        result=bytes(self.rx[:n]); del self.rx[:n]; return result
    def readline(self):
        if b'\n' not in self.rx: return self.read(len(self.rx))
        return self.read(self.rx.index(10)+1)
    def flush(self): pass
    def close(self): self.closed=True


class SessionTests(unittest.TestCase):
    def test_direct_and_bridge_record_and_stop(self):
        for link in ('direct','bridge'):
            with self.subTest(link=link),tempfile.TemporaryDirectory() as temp:
                fake=FakeSerial()
                args=SimpleNamespace(output=str(Path(temp)/'session'),port='TEST',baud=921600,
                                     link=link,mode='IQ',duration=.02,idle_timeout=1)
                session=capture(args,lambda *a,**kw:fake)
                self.assertEqual(session['status'],'captured')
                self.assertEqual(session['decoder']['frames'],1)
                self.assertTrue(fake.closed)
                self.assertTrue((Path(args.output)/'raw.bin').is_file())
                self.assertEqual(json.loads((Path(args.output)/'session.json').read_text())['status'],'captured')
                if link=='bridge': self.assertIn(b'AT+LINK=DIRECT\n',fake.commands)

    def test_mode_failure_is_saved_without_start(self):
        class RejectSerial(FakeSerial):
            def write(self,data):
                if data.startswith(b'AT+MODE='):
                    self.commands.append(data); self.rx.extend(b'AT+ERROR=MODE\n'); return len(data)
                return super().write(data)
        with tempfile.TemporaryDirectory() as temp:
            fake=RejectSerial(); args=SimpleNamespace(output=str(Path(temp)/'s'),port='TEST',baud=921600,
                     link='direct',mode='IQ',duration=.01,idle_timeout=1)
            with self.assertRaises(RuntimeError): capture(args,lambda *a,**kw:fake)
            self.assertNotIn(b'AT+START\n',fake.commands)
            self.assertEqual(json.loads((Path(args.output)/'session.json').read_text())['status'],'failed')

    def test_no_reply_timeout(self):
        with patch('capture_serial.time.monotonic',side_effect=[0,0,2]):
            with self.assertRaises(TimeoutError): expect(FakeSerial(),'UNKNOWN','AT+OK',timeout=1)

    def test_boot_retry(self):
        with patch('capture_serial.expect',side_effect=[TimeoutError(),None]) as handshake:
            sync(FakeSerial(),[])
            self.assertEqual(handshake.call_count,2)

    def test_bridge_is_released_even_when_radar_stop_fails(self):
        with tempfile.TemporaryDirectory() as temp:
            fake=FakeSerial(); args=SimpleNamespace(output=str(Path(temp)/'s'),port='TEST',baud=921600,
                     link='bridge',mode='IQ',duration=.01,idle_timeout=1)
            with patch('capture_serial.sync',side_effect=[None,TimeoutError('radar absent')]):
                with self.assertRaises(TimeoutError): capture(args,lambda *a,**kw:fake)
            self.assertIn(b'AT+LINK=DIRECT\n',fake.commands)
            self.assertTrue(fake.closed)


if __name__=='__main__': unittest.main()
