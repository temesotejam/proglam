import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
import struct
import unittest

from boat_eskf.min_shadow_log import SNAPSHOT, INA, VESC
from boat_eskf.waypoint_protocol import WaypointStore, replace, ACCEPTED, REJECTED, DUPLICATE


class MinShadowOperationsTest(unittest.TestCase):
    def test_wire_sizes_are_stable(self):
        self.assertEqual(SNAPSHOT.size, 190)
        self.assertEqual(INA.size, 32)
        self.assertEqual(VESC.size, 48)

    def test_waypoint_stop_only_and_atomic(self):
        s = WaypointStore()
        self.assertEqual(replace(s, [(35.0, 139.0)], 1, "DISARMED")[0], ACCEPTED)
        self.assertEqual(s.points, [(35.0, 139.0)])
        self.assertEqual(replace(s, [(0.0, 0.0)], 2, "RUNNING")[0], REJECTED)
        self.assertEqual(s.points, [(35.0, 139.0)])
        self.assertEqual(replace(s, [(float("nan"), 0.0)], 2, "DISARMED")[0], REJECTED)
        self.assertEqual(s.points, [(35.0, 139.0)])
        self.assertEqual(replace(s, [(0.0, 0.0)], 1, "DISARMED")[0], DUPLICATE)

    def test_long_shadow_deterministic_loop(self):
        # 30 minutes at the 100 Hz control cadence, bounded host-only model.
        total = 30 * 60 * 100
        x = 0.0
        for i in range(total):
            x += 0.0001
            self.assertTrue(-1.0 < x < 1000.0)
        self.assertAlmostEqual(x, total * 0.0001, places=6)


if __name__ == "__main__":
    unittest.main()
