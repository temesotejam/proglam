"""Host-side model of the STOP-only atomic waypoint register."""
from dataclasses import dataclass, field
import math

MAX_WAYPOINTS = 16
ACCEPTED, REJECTED, DUPLICATE = 0, 1, 2

@dataclass
class WaypointStore:
    revision: int = 0
    points: list[tuple[float, float]] = field(default_factory=list)

def validate(points: list[tuple[float, float]]) -> bool:
    return 0 < len(points) <= MAX_WAYPOINTS and all(
        math.isfinite(a) and math.isfinite(b) and -90 <= a <= 90 and -180 <= b <= 180
        for a, b in points)

def replace(store: WaypointStore, points: list[tuple[float, float]], revision: int,
            state: str) -> tuple[int, str]:
    if state == "RUNNING": return REJECTED, "RUNNING"
    if state == "E_STOP": return REJECTED, "E_STOP"
    if state != "DISARMED": return REJECTED, "STATE"
    if revision <= store.revision: return DUPLICATE, "REVISION"
    if not validate(points): return REJECTED, "RANGE"
    # The assignment is intentionally last: malformed requests cannot partially
    # change the active register.
    store.points = list(points)
    store.revision = revision
    return ACCEPTED, "NONE"
