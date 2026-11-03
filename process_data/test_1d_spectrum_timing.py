import unittest
import numpy as np
from analyze_1d_iq import spectrum_segment, time_axis


def metadata(values):
    return [dict(interval_ms=x) for x in values]


class SpectrumTimingTests(unittest.TestCase):
    def test_missing_tail_keeps_prefix_and_input_unchanged(self):
        meta=metadata([100]*985+[None])
        sl,t,info=spectrum_segment(meta)
        self.assertEqual((sl.start,sl.stop),(0,985))
        self.assertAlmostEqual(t[-1],98.4)
        self.assertEqual(info['excluded_frames'],1)
        self.assertEqual(len(meta),986)
        self.assertIsNone(time_axis(meta)[1]['fps'])

    def test_middle_missing_not_spliced(self):
        sl,t,info=spectrum_segment(metadata([100,100,None,100,100,100]))
        self.assertEqual((sl.start,sl.stop),(2,6))
        np.testing.assert_allclose(t,[0,.1,.2,.3])

    def test_rounding_boundary_accepted(self):
        sl,t,info=spectrum_segment(metadata([100,100,98,100,101,100]))
        self.assertTrue(info['uniform'])
        self.assertEqual(info['used_frames'],6)

    def test_first_interval_not_needed(self):
        sl,t,info=spectrum_segment(metadata([None,100,100]))
        self.assertEqual(info['used_frames'],3)
        self.assertAlmostEqual(t[-1],.2)

    def test_no_timing_not_invented(self):
        sl,t,info=spectrum_segment(metadata([None]*8))
        self.assertIsNone(info['fps'])
        self.assertEqual(len(t),0)

    def test_large_gap_splits(self):
        sl,t,info=spectrum_segment(metadata([100,100,1000,100,100,100]))
        self.assertEqual((sl.start,sl.stop),(2,6))
        self.assertEqual(info['large_gap_frames'],[2])

    def test_manual_fps_explicit_and_validated(self):
        sl,t,info=spectrum_segment(metadata([None]*10),10)
        self.assertEqual(info['used_frames'],10)
        self.assertAlmostEqual(t[-1],.9)
        for bad in [0,-1,np.nan,np.inf]:
            with self.assertRaises(ValueError):spectrum_segment(metadata([None]*10),bad)

    def test_known_frequency_survives_tail_exclusion(self):
        meta=metadata([100]*400+[None])
        sl,t,info=spectrum_segment(meta)
        source_t=np.arange(401)/10
        iq=np.exp(1j*.7*np.sin(2*np.pi*.25*source_t))
        phase=np.unwrap(np.angle(iq[sl]))
        phase-=np.polyval(np.polyfit(t,phase,1),t)
        freq=np.fft.rfftfreq(len(t),.1)
        peak=freq[np.abs(np.fft.rfft(phase*np.hanning(len(t)))).argmax()]
        self.assertAlmostEqual(peak,.25,places=6)


if __name__=='__main__':unittest.main()
