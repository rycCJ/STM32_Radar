"""Cross-check real C firmware output against Python decoder and C observer."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'process_data'))
from capture_protocol import Decoder


class NativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc=os.environ.get('RADAR_TEST_CC') or shutil.which('gcc')
        if not cc: raise unittest.SkipTest('GCC not installed; ARMCC builds remain separate')
        out=ROOT/'build/host_tests'; out.mkdir(parents=True,exist_ok=True)
        radar=ROOT/'firmware/radar/proj/V1.1.2_chirpmean_44+86/App'
        controller=ROOT/'firmware/controller/User'
        native=ROOT/'tests/native'
        cls.encoder=out/'encoder.exe'; cls.observer=out/'observer.exe'
        subprocess.run([cc,'-std=c99','-Wall','-Wextra','-Werror','-I'+str(native),'-I'+str(radar/'Inc'),
                        str(radar/'Capture_Output.c'),str(native/'encoder.c'),'-o',str(cls.encoder)],check=True,capture_output=True)
        subprocess.run([cc,'-std=c99','-Wall','-Wextra','-Werror','-I'+str(native),'-I'+str(controller),
                        str(controller/'controller_points.c'),str(native/'observer.c'),'-o',str(cls.observer)],check=True,capture_output=True)

    def encode(self,mode): return subprocess.check_output([str(self.encoder),mode])

    def test_real_encoder_all_modes(self):
        for mode in ('ADC','IQ','POINT','LEGACY','ADCOLD'):
            with self.subTest(mode=mode):
                decoder=Decoder({'LEGACY':'legacy','ADCOLD':'adc-legacy'}.get(mode,'new'))
                frames=decoder.feed(self.encode(mode))
                self.assertEqual(len(frames),1)
                self.assertEqual(decoder.finish()['trailing_bytes'],0)
                self.assertEqual(decoder.stats['crc_errors'],0)
                if mode=='IQ':
                    self.assertEqual(frames[0].values()[1][-1],1149)
                    self.assertEqual(frames[0].ticks,3200)
                    self.assertAlmostEqual(frames[0].bin_m,.050041,places=5)
                if mode in ('ADC','ADCOLD'): self.assertEqual(frames[0].values()[-1],511)

    def test_controller_observes_both_point_formats(self):
        for mode in ('POINT','LEGACY'):
            count=subprocess.check_output([str(self.observer)],input=self.encode(mode))
            self.assertEqual(count.strip(),b'1')

    def test_controller_rejects_corrupt_new_frame(self):
        data=bytearray(self.encode('POINT')); data[48]^=1
        count=subprocess.check_output([str(self.observer)],input=data)
        self.assertEqual(count.strip(),b'0')


if __name__=='__main__': unittest.main()
