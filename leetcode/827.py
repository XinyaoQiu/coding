from typing import *

class Solution:
    def find(self, p: List[int], x: int) -> int:
        while x != p[x]:
            p[x] = p[p[x]]
            x = p[x]
        return x

    def union(self, p: List[int], w: List[int], x: int, y: int):
        root_x, root_y = self.find(p, x), self.find(p, y)
        if root_x == root_y:
            return
        p[root_x] = root_y
        w[root_y] += w[root_x]
        w[root_x] = 0

    def largestIsland(self, grid: List[List[int]]) -> int:
        n = len(grid)
        N = n * n
        p = [i for i in range(N)]
        w = [1] * N
        dirs = [(0, 1), (1, 0), (0, -1), (-1, 0)]
        for i in range(n):
            for j in range(n):
                if grid[i][j] == 1:
                    for di, dj in dirs[:2]:
                        ni, nj = i + di, j + dj
                        if ni < n and nj < n and grid[ni][nj] == 1:
                            self.union(p, w, i * n + j, ni * n + nj)
        ans = 0
        for i in range(n):
            for j in range(n):
                if grid[i][j] == 0:
                    st = set()
                    curr = 1
                    for di, dj in dirs:
                        ni, nj = i + di, j + dj
                        if 0 <= ni < n and 0 <= nj < n and grid[ni][nj] == 1:
                            root = self.find(p, ni * n + nj)
                            if root not in st:
                                st.add(root)
                                curr += w[root]
                    ans = max(ans, curr)
        return ans if ans > 0 else N
                    


