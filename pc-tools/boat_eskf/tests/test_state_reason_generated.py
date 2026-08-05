import subprocess
import sys
import unittest
from pathlib import Path

class StateReasonGenerationTest(unittest.TestCase):
    def test_generated_files_are_fresh(self):
        root=Path(__file__).resolve().parents[3]
        subprocess.run([sys.executable,str(root/'shared'/'proposal_min'/'tools'/'generate_state_reason.py'),'--check'],cwd=root,check=True)

if __name__=='__main__': unittest.main()
