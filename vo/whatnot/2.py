from typing import *
from collections import defaultdict, Counter

"""
logs = [["200", "user_1", "resource_5", ...], ...]
ans = {"_START_": {"resource_1": 0.6, ...}, ...}

user_resources = {"user_1": [(1, "resource_1"), (2, "resource_2"), ...], ...}
user_resources = {"user_1": ["_START_", "resource_1", ..., "_END_"]}

resource_path = {"resource_1": ["resource_2", "_END_", ...], ...}
ans
"""

# O(N * log(sqrt(N))) 
def transition_graph(logs: List[List[str]]) -> Dict[str, Dict[str, float]]:
    user_resources = defaultdict(list)
    for time, user, resource in logs:
        user_resources[user].append((int(time), resource))
    for user in user_resources:
        r_l = user_resources[user] 
        r_l.sort()
        user_resources[user] = ["_START_"] + [x[1] for x in r_l] + ["_END_"]
    resource_path = defaultdict(list)
    for path in user_resources.values():
        for i in range(0, len(path) - 1):
            resource_path[path[i]].append(path[i + 1])
    ans = {}
    for resource, path in resource_path.items():
        counter = Counter(path)
        ans[resource] = {k: round(v / len(path), 2) for k, v in counter.items()}
    return ans

logs1 = [
    ["200", "user_1", "resource_5"],
    ["3", "user_1", "resource_1"],
    ["620", "user_1", "resource_1"],
    ["620", "user_3", "resource_1"],
    ["34", "user_6", "resource_2"],
    ["95", "user_9", "resource_1"],
    ["416", "user_6", "resource_1"],
    ["58523", "user_3", "resource_1"],
    ["53760", "user_3", "resource_3"],
    ["58522", "user_22", "resource_1"],
    ["100", "user_3", "resource_6"],
    ["400", "user_6", "resource_2"],
]

print(transition_graph(logs1))
    