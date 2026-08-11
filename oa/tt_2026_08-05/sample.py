import string
from collections import defaultdict

def func1(s: str):
    lower = upper = 0
    for ch in s:
        if ch.islower():
            lower += 1
        elif ch.isupper():
            upper += 1
    return lower - upper

def func2(arr):
    vowels = { 'a', 'e', 'i', 'o', 'u', 'A', 'E', 'I', 'O', 'U' }
    ans = []
    for s in arr:
        if len(s) >= 2 and s[0] in vowels and s[-1] in vowels:
            ans.append(s[0] + s[-2:0:-1] + s[-1])
        else:
            ans.append(s)
    return ans

def func3(board):
    m, n = len(board), len(board[0])
    dirs = [(0, -1), (-1, 0), (1, 0), (0, 1)]
    while True:
        kill = [[False] * n for _ in range(m)]
        found = False
        for i in range(m):
            for j in range(n):
                v = board[i][j]
                if v == 0:
                    continue
                same = [(i + di, j + dj) for di, dj in dirs 
                        if 0 <= i + di < m and 0 <= j + dj < n and board[i+di][j+dj] != 0]
                if len(same) >= 2:
                    kill[i][j] = True
                    for ni, nj in same:
                        kill[ni][nj] = True
                    found = True
        if not found:
            return board
        for j in range(n):
            keep = [board[i][j] for i in range(m) if not kill[i][j] and board[i][j] != 0]
            top = n - len(keep)
            for i in range(m):
                board[i][j] = 0 if i < top else keep[i - top] 

def func4(travelPhotos):
    edges = defaultdict(set)
    for a, b in travelPhotos:
        edges[a].add(b)
        edges[b].add(a)
    start = None
    for u, neighb in edges.items():
        if len(neighb) == 1:
            start = u
            break
    visited = set([start])
    ans = []
    def dfs(u):
        ans.append(u)
        for v in edges[u]:
            if v not in visited:
                visited.add(v)
                dfs(v)
    dfs(start)
    return ans

print(func4([[3, 5], [1, 4], [2, 4], [1, 5]]))

def sort_spiral_layers(matrix):
    if not matrix or not matrix[0]:
        return matrix
    m, n = len(matrix), len(matrix[0])
    top = left = 0
    bottom, right = m - 1, n - 1
    while top <= bottom and left <= right:
        coords = []
        for j in range(left, right+1):
            coords.append((top, j))
        for i in range(top + 1, bottom + 1):
            coords.append((i, right))
        if left < right and top < bottom:
            for j in range(right - 1, left - 1, -1):
                coords.append((bottom, j))
            for i in range(bottom - 1, top, -1):
                coords.append((i, left))
        vals = sorted(matrix[i][j] for i, j in coords)
        for (i, j), v in zip(coords, vals):
            matrix[i][j] = v
        top += 1; bottom -= 1; left += 1; right -= 1
    return matrix

from collections import Counter
from math import comb

def solution(a):
    counter = Counter()
    for x in a:
        x = str(x)
        base = min(x[i:] + x[:i] for i in range(len(x)))
        counter[base] += 1
    return sum(comb(c, 2) for c in counter.values())

def solution(memory, queries):
    n = len(memory)
    counter = 1
    ans = []
    blocks = {}
    for op, val in queries:
        if op == 0:
            start = -1
            run = 0
            for i in range(n):
                if memory[i] == 0:
                    run += 1
                    if run == val:
                        start = i - val + 1
                        blocks[counter] = (start, val)
                        counter += 1
                        for j in range(start, start + val):
                            memory[j] = 1
                        break
                else:
                    run = 0
            ans.append(start)
        else:
            if val not in blocks:
                ans.append(-1)
                continue
            start, length = blocks.pop(val)
            ans.append(length)
            for i in range(start, start + length):
                memory[i] = 0
    return ans

def solution(buildings, queries):
    alive = set(buildings)
    count = sum(1 for x in alive if x - 1 not in alive)
    ans = []
    for destroy in queries:
        if destroy in alive:
            left, right = destroy - 1 in alive, destroy + 1 in alive
            if left and right:
                count += 1
            elif not left and not right:
                count -= 1
            alive.remove(destroy)
        ans.append(count)
    return ans
    