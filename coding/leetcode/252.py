from typing import *

class Solution:
    def canAttendMeetings(self, intervals: List[List[int]]) -> bool:
        count = 0
        sorted_intervals = sorted(intervals, key=lambda x: x[0])
        for i in range(len(sorted_intervals) - 1):
            if sorted_intervals[i + 1][0] < sorted_intervals[i][1]:
                return False
        return True