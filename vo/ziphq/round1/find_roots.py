from collections import defaultdict, deque
import json

def func1(json_str):
    blocks = json.loads(json_str)
    roots = [b['id'] for b in blocks if b['parent'] is None]
    print(roots)
    children = defaultdict(list)
    for b in blocks:
        if b['parent'] is not None:
            children[b['parent']].append(b['id'])
    all_paths = []
    for root in roots:
        q = deque([root])
        path = []
        while q:
            node = q.popleft()
            path.append(node)
            if node in children:
                for child in children[node]:
                    q.append(child)
        all_paths.append(path)
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