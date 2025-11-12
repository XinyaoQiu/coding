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
    q = deque(roots)
    
    

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
func1(json_str)