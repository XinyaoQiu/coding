from typing import *
from collections import defaultdict

def dfs(edges, mid_courses, heights, u, depth):
    if len(edges[u]) == 0:
        heights[u] = set([0])
        return
    if u not in heights:
        heights[u] = set()
        for v in edges[u]:
            dfs(edges, mid_courses, heights, v, depth + 1)
            for h in heights[v]:
                heights[u].add(h + 1)
    if depth in heights[u] or depth + 1 in heights[u]:
        mid_courses.add(u)

def func(all_courses: List[List[str]]) -> List[str]:
    edges = defaultdict(list)
    indegs = defaultdict(int)
    mid_courses = set()
    heights = {}
    for x, y in all_courses:
        edges[x].append(y)
        indegs[y] += 1
    for course in list(edges.keys()):
        if indegs[course] == 0:
            dfs(edges, mid_courses, heights, course, 0)
    return list(mid_courses)

all_courses = [
    ["Logic", "COBOL"],
    ["Data Structures", "Algorithms"],
    ["Creative Writing", "Data Structures"],
    ["Algorithms", "COBOL"],
    ["Intro to Computer Science", "Data Structures"],
    ["Logic", "Compilers"],
    ["Data Structures", "Logic"],
    ["Creative Writing", "System Administration"],
    ["Databases", "System Administration"],
    ["Creative Writing", "Databases"],
    ["Intro to Computer Science", "Graphics"],
]

print(func(all_courses))
    

