import json
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

from boat_eskf.proposal_benchmark import MODES, run_benchmark
from boat_eskf.proposal_replay import run_replay_suite


class TestProposalEvaluation(unittest.TestCase):
    def test_fixed_length_all_modes_are_finite(self):
        for mode in MODES:
            result = run_benchmark(mode, 10_000)
            self.assertEqual(result["iterations"], 10_000)
            self.assertEqual(result["output_count"], result["finite_output_count"])
            self.assertTrue(result["host_timing_only"])

    def test_replay_covers_normal_path_and_faults(self):
        result = run_replay_suite("FULL")
        self.assertTrue(result["normal_reproducible"])
        self.assertTrue(result["all_faults_safe"])
        self.assertEqual(result["normal"]["final_state"], "GOAL_STOP")
        self.assertIn("LOS_NAVIGATION", result["normal_phase_coverage"])
        for name, fault in result["faults"].items():
            self.assertTrue(fault["safe_output_after_stop"], name)
            self.assertTrue(fault["all_shadow_finite"], name)


if __name__ == "__main__":
    unittest.main()
