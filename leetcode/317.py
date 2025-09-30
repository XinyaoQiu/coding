from collections import deque
from math import inf
from typing import List

class Solution:
    def getShortedPath(self, grid, i, j, num_ones):
        dirs = [(-1, 0), (0, -1), (1, 0), (0, 1)]
        queue = deque([(i, j)])
        m, n = len(grid), len(grid[0])
        total, steps = 0, 0
        visited = [[0] * n for _ in range(m)]
        visited[i][j] = 1
        while queue and num_ones > 0:
            for _ in range(len(queue)):
                x, y = queue.popleft()
                if grid[x][y] == 1:
                    total += steps
                    num_ones -= 1
                    continue
                for dx, dy in dirs:
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < m and 0 <= ny < n and not visited[nx][ny] and grid[nx][ny] != 2:
                        visited[nx][ny] = 1
                        queue.append((nx, ny))
            steps += 1
        if num_ones > 0:
            for i in range(m):
                for j in range(n):
                    if grid[i][j] == 0 and visited[i][j]:
                        grid[i][j] = 2
            return inf
        return total


    def shortestDistance(self, grid: List[List[int]]) -> int:
        m, n = len(grid), len(grid[0])
        result = inf
        num_ones = 0
        for i in range(m):
            for j in range(n):
                if grid[i][j] == 1:
                    num_ones += 1
        for i in range(m):
            for j in range(n):
                if grid[i][j] == 0:
                    result = min(result, self.getShortedPath(grid, i, j, num_ones))
        return result if result != inf else -1
        


        