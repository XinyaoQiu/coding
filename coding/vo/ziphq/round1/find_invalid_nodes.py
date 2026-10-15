from collections import defaultdict, deque

nodes = [
    [2, 1, 1], 
    [3, 1, 1], 
    [4, 2, 2],
    [5, 2, 3],
    [6, 5, 3],
    [9, 5, 2],
    [8, 7, 1],
]

def func1(nodes):
    ids = set()
    info = {}
    children = defaultdict(list)
    for nid, parent, level in nodes:
        ids.add(nid)
        ids.add(parent)
        info[nid] = level
        children[parent].append(nid)
    roots = set()
    for id in ids:
        if id not in info:
            roots.add(id)
    invalid = set()
    level = 0
    q = deque(list(roots))
    while q:
        for _ in range(len(q)):
            node = q.popleft()
            if node not in roots and info[node] != level:
                invalid.add(node)
            for child in children[node]:
                q.append(child)
        level += 1
    print(invalid)
    all_paths = []
    def dfs(root, path):
        if not children[root]:
            all_paths.append(path.copy())
            return
        for child in children[root]:
            if child not in invalid:
                path.append(child)
                dfs(child, path)
                path.pop()
    for root in roots:
        path = [root]
        dfs(root, path)
    print(all_paths)

func1(nodes)
