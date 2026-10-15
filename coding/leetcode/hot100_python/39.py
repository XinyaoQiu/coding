from typing import *

class Solution:
    def backtrack(self, ans: List[List[int]], candidates: List[int], path: List[int], target: int, i: int):
        if target == 0:
            ans.append(path.copy())
            return
        for j in range(i, len(candidates)):
            if candidates[j] > target:
                return
            path.append(candidates[j])
            self.backtrack(ans, candidates, path, target - candidates[j], j)
            path.pop()

    def combinationSum(self, candidates: List[int], target: int) -> List[List[int]]:
        ans, path = [], []
        candidates.sort()
        self.backtrack(ans, candidates, path, target, 0)
        return ans