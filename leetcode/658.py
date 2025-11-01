from bisect import bisect_left
from typing import *

class Solution:
    def findClosestElements(self, arr: List[int], k: int, x: int) -> List[int]:
        right = bisect_left(arr, x)
        left = right - 1
        for _ in range(k):
            if left == -1 or (right < len(arr) and x - arr[left] > arr[right] - x):
                right += 1
            else:
                left -= 1
        return arr[left+1:right]