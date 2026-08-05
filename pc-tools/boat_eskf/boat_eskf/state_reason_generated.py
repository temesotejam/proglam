# generated from shared/proposal_min/state_reason.json; do not edit
STATES={'DISARMED': 0, 'RUNNING': 1, 'E_STOP': 2, 'FAULT': 3}
REASONS={'NONE': 0, 'STOP': 1, 'E_STOP': 2, 'HEARTBEAT_TIMEOUT': 3, 'GNSS_INVALID': 4, 'GNSS_STALE': 5, 'IMU_INVALID': 6, 'IMU_STALE': 7, 'TOF_INVALID': 8, 'TOF_STALE': 9, 'NONFINITE': 10, 'VESC_FAULT': 11}
REASON_NAMES={0: 'NONE', 1: 'STOP', 2: 'E_STOP', 3: 'HEARTBEAT_TIMEOUT', 4: 'GNSS_INVALID', 5: 'GNSS_STALE', 6: 'IMU_INVALID', 7: 'IMU_STALE', 8: 'TOF_INVALID', 9: 'TOF_STALE', 10: 'NONFINITE', 11: 'VESC_FAULT'}
ALLOWED_TRANSITIONS=[(0, 0), (0, 1), (0, 2), (0, 3), (1, 0), (1, 1), (1, 2), (1, 3), (2, 0), (2, 2), (2, 3), (3, 0), (3, 3)]
FAULT_REASONS=(3, 4, 5, 6, 7, 8, 9, 10, 11)
