from collections import defaultdict, deque
import json

def func1(json_str):
    blocks = json.loads(json_str)
    roots = [b['id'] for b in blocks if b['parent'] is None]
    print(roots)
    children = defaultdict(list) # {1: [2, 3], 2: [4, 5]}
    for b in blocks:
        if b['parent'] is not None:
            children[b['parent']].append(b['id'])
    print(dict(children))
    all_paths = []
    def dfs(root, path):
        if root not in children:
            all_paths.append(path.copy())
            return
        for child in children[root]:
            path.append(child)
            dfs(child, path)
            path.pop()
    for root in roots:
        path = [root]
        dfs(root, path)
    return all_paths
    

json_str = '''
[
    {"id": 1, "parent": null},
    {"id": 2, "parent": 1},
    {"id": 3, "parent": 1},
    {"id": 4, "parent": 2},
    {"id": 5, "parent": 2},
    {"id": 6, "parent": null}
]
'''
print(func1(json_str))