from typing import *
from collections import deque

class Solution:
    def openLock(self, deadends: List[str], target: str) -> int:
        deadends_set = set(deadends)
        if '0000' in deadends_set:
            return -1
        q = deque(['0000'])
        parents = {'0000': None}
        step = 0
        while q:
            for _ in range(len(q)):
                curr = q.popleft()
                if curr == target:
                    path = []
                    while curr is not None:
                        path.append(curr)
                        curr = parents[curr]
                    path.reverse()
                    print(path)
                    return step
                for i, c in enumerate(curr):
                    for sign in [1, -1]:
                        next = curr[:i] + chr(ord('0') + (ord(c) - ord('0') + sign) % 10) + curr[i+1:]
                        if next not in parents and next not in deadends_set:
                            parents[next] = curr
                            q.append(next)
            step += 1
        return -1
    
sol = Solution()
sol.openLock(["0201","0101","0102","1212","2002"], "0202")
