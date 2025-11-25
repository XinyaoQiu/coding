import math

class BalloonFestival:
    def __init__(self, yourBalloonNames):
        # Team balloons set
        self.team = set(yourBalloonNames)
        self.order = {name: i for i, name in enumerate(yourBalloonNames)}

        # Balloon state: name -> {...}
        self.balloons = {}
        # Each balloon record:
        # {
        #   "altitude": None or float,
        #   "stable": bool,
        #   "recover_after": float or None,
        #   "last_altitude": float or None,
        # }

        # Wind centers: altitude → windSpeed
        self.wind_centers = {}

        self.last_timestamp = -1.0

    # -------- Utility --------

    def _wind_at(self, h):
        """Compute total wind at altitude h."""
        total = 0.0
        for center_alt, w in self.wind_centers.items():
            total += w / (1 + ((h - center_alt) / 100.0)**2)
        return total

    def _ensure_timestamp(self, ts):
        if ts <= self.last_timestamp:
            return False
        self.last_timestamp = ts
        return True

    def _recompute_all(self, timestamp):
        """
        After wind update, reevaluate each balloon’s stability.
        """
        for name, b in self.balloons.items():
            if b["altitude"] is None:
                continue  # On ground, ignore

            h = b["altitude"]
            wind = self._wind_at(h)

            if wind > 15:
                b["stable"] = False
                b["recover_after"] = None
            else:
                # wind safe
                if b["stable"]:
                    continue  # already stable
                else:
                    # unstable -> try recovery logic
                    if b["recover_after"] is None:
                        # Start new recovery window
                        b["recover_after"] = timestamp + 300
                    else:
                        if timestamp >= b["recover_after"]:
                            # Recovery successful
                            b["stable"] = True
                            b["recover_after"] = None

    # -------- Operations --------

    def BalloonAscended(self, timestamp, balloonName, altitude):
        if not self._ensure_timestamp(timestamp):
            return False
        if balloonName not in self.team:
            return False
        if altitude <= 0:
            return False

        # Initialize balloon if missing
        if balloonName not in self.balloons:
            self.balloons[balloonName] = {
                "altitude": None,
                "stable": True,
                "recover_after": None,
                "last_altitude": None,
            }

        b = self.balloons[balloonName]

        # Ascend
        b["last_altitude"] = altitude
        b["altitude"] = altitude
        b["stable"] = True   # stable by default
        b["recover_after"] = None

        # Immediately check wind instability
        if self._wind_at(altitude) > 15:
            b["stable"] = False

        return True

    def BalloonDescended(self, timestamp, balloonName):
        if not self._ensure_timestamp(timestamp):
            return False
        if balloonName not in self.team:
            return False
        if balloonName not in self.balloons:
            return False

        b = self.balloons[balloonName]
        if b["altitude"] is None:
            return False  # already on ground

        # Descend: reset state
        b["altitude"] = None
        b["stable"] = True
        b["recover_after"] = None
        b["last_altitude"] = None
        return True

    def SetWindSpeed(self, timestamp, altitude, windSpeed):
        if not self._ensure_timestamp(timestamp):
            return False
        if not (0 < altitude < 2**15):
            return False
        if not (0 <= windSpeed < 2**5):
            return False

        # Update wind center
        self.wind_centers[altitude] = windSpeed

        # Recompute stability for all balloons
        self._recompute_all(timestamp)
        return True

    def InspectBalloons(self, timestamp):
        if timestamp <= self.last_timestamp:
            return []
        self.last_timestamp = timestamp

        # Compute current stability in case any recovery completes exactly at this moment
        # (needed if a balloon waited exactly 300s at a stable wind level)
        for name, b in self.balloons.items():
            if b["altitude"] is None:
                continue
            h = b["altitude"]
            wind = self._wind_at(h)

            if wind > 15:
                b["stable"] = False
                b["recover_after"] = None
            else:
                if not b["stable"]:
                    if b["recover_after"] is None:
                        b["recover_after"] = timestamp + 300
                    else:
                        if timestamp >= b["recover_after"]:
                            b["stable"] = True
                            b["recover_after"] = None

        # Find highest stable competitor altitude
        highest_stable_alt = -1

        for name, b in self.balloons.items():
            if name in self.team:
                continue
            if b["altitude"] is not None and b["stable"]:
                highest_stable_alt = max(highest_stable_alt, b["altitude"])

        # Now find team balloons ≥ this altitude and stable
        result = []
        for name in self.team:
            if name not in self.balloons:
                continue
            b = self.balloons[name]
            if b["altitude"] is not None and b["stable"] and b["altitude"] >= highest_stable_alt:
                result.append(name)

        # Must preserve original order
        result.sort(key=lambda x: self.order[x])

        return result
