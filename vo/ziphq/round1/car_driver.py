from collections import deque, defaultdict
from typing import *

def func1(cab: Tuple[int, int], destination: Tuple[int, int], barriers):
    sx, sy = cab
    barriers_set = set(barriers)
    dirs = [(0, 1, 'N'), (0, -1, 'S'), (1, 0, 'E'), (-1, 0, 'W')]
    q = deque([(sx, sy, 0)])
    parents = {} # {(3, 4): (2, 4, 'E')}
    visited = set([(sx, sy)])
    while q:
        for _ in range(len(q)):
            x, y, step = q.popleft()
            if (x, y) == destination:
                path = ""
                curr = (x, y)
                while curr in parents:
                    path += parents[curr][2]
                    curr = (parents[curr][0], parents[curr][1])
                return path[::-1], step
            for dx, dy, dir in dirs:
                nx, ny = x + dx, y + dy
                if (nx, ny) not in barriers_set and (nx, ny) not in visited:
                    visited.add((nx, ny))
                    q.append((nx, ny, step + 1))
                    parents[(nx, ny)] = (x, y, dir)

print(func1((3, 0), (8, 3), [(7,0), (7,1), (7, 2), (7,3), (7,4), (8,2)]))

class Parade:
    def __init__(self, start: Tuple[int, int], dir: str):
        self.start = start
        self.dir = dir

parades = [Parade((3, 1), 'E'), Parade((3, 2), 'E'), Parade((3, 3), 'E'), Parade((4, 0), 'N'), Parade((5, 1), 'N')]

def parade_at(x, y, t, mp):
    k = t // 2
    for (dx, dy), starts in mp.items():
        ox = x - dx * k
        oy = y - dy * k
        if (ox, oy) in starts:
            return True
    return False

def func2(cab: Tuple[int, int], destination: Tuple[int, int], parades: List[Parade]):
    sx, sy = cab
    tx, ty = destination
    DIR_MAP = {'N': (0, 1), 'S': (0, -1), 'E': (1, 0), 'W': (-1, 0)}
    mp = defaultdict(set)
    for p in parades:
        mp[DIR_MAP[p.dir]].add(p.start)
    q = deque([(sx, sy, 0)])
    visited = set([(sx, sy, 0)])
    dirs = [(1, 0), (-1, 0), (0, 1), (0, -1)]
    while q:
        x, y, t = q.popleft()
        if parade_at(x, y, t, mp):
            continue
        if (x, y) == (tx, ty):
            return t
        for dx, dy in dirs:
            nx, ny = x + dx, y + dy
            nt = t + 1
            if parade_at(nx, ny, nt, mp):
                continue
            if parade_at(nx, ny, t, mp) and parade_at(x, y, nt, mp):
                continue
            state = (nx, ny, nt)
            if state not in visited:
                visited.add(state)
                q.append(state)
    return None

print(func2((3, 0), (8, 3), parades))