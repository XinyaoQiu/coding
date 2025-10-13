from typing import *
from collections import defaultdict

class Solution:
    def minMeetingRooms(self, intervals: List[List[int]]) -> int:
        events = [(i[0], 1) for i in intervals] + [(i[1], -1) for i in intervals]
        events.sort()
        total = ans = 0
        for _, e in events:
            total += e
            ans = max(ans, total)
        return ans
