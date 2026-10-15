from collections import deque
from typing import List
import sys

def best_transport_mode(grid, modes, times, costs) -> str:
    m, n = len(grid), len(grid[0])
    q = deque([])
    dirs = [(0, -1), (-1, 0), (1, 0), (0, 1)]
    visited = [[False] * n for _ in range(m)]
    for i in range(m):
        for j in range(n):
            if grid[i][j] == 'S':
                for di, dj in dirs:
                    ni, nj = i + di, j + dj
                    if 0 <= ni < m and 0 <= nj < n and grid[ni][nj].isdigit():
                        mode = int(grid[ni][nj]) - 1
                        q.append((ni, nj, mode, 1))
                        visited[ni][nj] = True
    best_time, best_cost, best_mode = sys.maxsize, sys.maxsize, ''
    print(q)
    while q:
        x, y, mode, step = q.popleft()
        for dx, dy in dirs:
            nx, ny = x + dx, y + dy
            if 0 <= nx < m and 0 <= ny < n:
                if grid[nx][ny] == str(mode + 1) and not visited[nx][ny]:
                    visited[nx][ny] = True
                    q.append((nx, ny, mode, step + 1))
                elif grid[nx][ny] == 'D':
                    time_total = (step + 1) * times[mode]
                    cost_total = (step + 1) * costs[mode]
                    print((time_total, cost_total, mode))
                    if (time_total < best_time) or (time_total == best_time and cost_total < best_cost):
                        best_time, best_cost, best_mode = time_total, cost_total, modes[mode]
    return best_time, best_cost, best_mode
                

grid = [
    ['S', '1', '1', '1', 'D', '2', '1', 'X'],
    ['2', '2', '2', '2', '2', '2', '1', '2'],
    ['X', 'X', '2', '2', '2', '2', '2', '2'],
]

modes = ["walk", "bus"]
times = [5, 2]
costs = [0, 1]

print(best_transport_mode(grid, modes, times, costs))