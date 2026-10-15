from typing import *
from collections import defaultdict

class Solution:
    def subarraySum(self, nums: List[int], k: int) -> int:
        curr = ans = 0
        mp = defaultdict(int)
        mp[0] = 1
        for n in nums:
            curr += n
            ans += mp[curr - k]
            mp[curr] += 1
        return ans
            