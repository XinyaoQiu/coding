from functools import reduce
from typing import *

class Solution:
    def dailyTemperatures(self, temperatures: List[int]) -> List[int]:
        stk = []
        ans = [0] * len(temperatures)
        for i, t in enumerate(temperatures):
            while stk and t > temperatures[stk[-1]]:
                j = stk.pop()
                ans[j] = i - j
            stk.append(i)
        return ans
