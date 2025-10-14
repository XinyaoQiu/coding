from typing import *
from heapq import *

class Solution:
    def findMaximizedCapital(self, k: int, w: int, profits: List[int], capital: List[int]) -> int:
        if w >= max(capital):
            return w + sum(nlargest(k, profits))
        pq = []
        i = 0
        arr = [(capital[i], profits[i]) for i in range(len(profits))]
        arr.sort()
        for _ in range(k):
            while i < len(profits) and arr[i][0] <= w:
                heappush(pq, -arr[i][1])
                i += 1
            if not pq:
                break
            w -= heappop(pq)
        return w
        
        