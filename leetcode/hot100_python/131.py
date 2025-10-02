from typing import *

class Solution:
    def partition(self, s: str) -> List[List[str]]:
        if s == "":
            return []
        t = "#".join("^" + s + "$")
        # ti = 2 * si + 2, si = ti // 2 - 1
        # si from 0 to len(s) - 1, ti from 2 to 2 * len(s)
        half_len = [0] * (len(t) - 2)
        half_len[1] = 1
        box_m = box_r = 0
        for i in range(2, len(half_len)):
            hl = 1
            if i < box_r:
                hl = min(half_len[2 * box_m - i], box_r - i)
            while t[i - hl] == t[i + hl]:
                hl += 1
                box_m = i
                box_r = i + hl
            half_len[i] = hl
        #  i <= s <= j  -> 2 * i + 2 <= t <= 2 * j + 2 -> half_len[i + j + 2] >= j - i + 1
        path = []
        ans = []
        def backtrack(i):
            if i == len(s):
                ans.append(path.copy())
                return
            for j in range(i, len(s)):
                if half_len[i + j + 2] >= j - i + 1:
                    path.append(s[i:j+1])
                    backtrack(j + 1)
                    path.pop()
        backtrack(0)
        return ans


