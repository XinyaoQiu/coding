from typing import *
from collections import defaultdict
from itertools import combinations

"""
input: performences: List[List[str]], period: int
output: Dict[str, Dict[str, int]]

singer_roles: {singer: {role: year, ...}, ...}
cooc: {role: {other_role: count, ...}, ...}

singer_roles.values(): List[Dict[str, int]]
iterate over
"""

def cooccurrences(performances: List[List[str]], period: int) -> Dict[str, Optional[Dict[str, int]]]:
    singer_roles = defaultdict(dict)
    cooc = defaultdict(lambda: defaultdict(int))
    all_roles = set()

    for singer, role, year in performances:
        singer_roles[singer][role] = int(year)
        all_roles.add(role)

    for roles in singer_roles.values():
        for x, y in combinations(roles.keys(), 2):
            if abs(roles[x] - roles[y]) <= period:
                cooc[x][y] += 1
                cooc[y][x] += 1
    
    for role in all_roles:
        cooc.setdefault(role, None)
    
    return {k: dict(v) if isinstance(v, dict) else v for k, v in cooc.items()}
  

performances1 = [
    ["Alice", "Role_A", "2010"],
    ["Alice", "Role_B", "2013"],
    ["Alice", "Role_C", "2016"],
]
performances2 = [
    ["Daphne", "Role_D", "2013"],
    ["Alice", "Role_A", "2010"],
    ["Alice", "Role_B", "2013"],
    ["Alice", "Role_C", "2016"],
]
performances3 = [
    [ "Alice", "Role_E", "2015" ],
    [ "Bob", "Role_F", "2015" ],
    [ "Charles", "Role_G", "2015" ],
    [ "Daphne", "Role_D", "2010" ],
    [ "Alice", "Role_H", "2016" ],
    [ "Bob", "Role_I", "2016" ],
    [ "Charles", "Role_J", "2017" ],
    [ "Eric", "Role_F", "2019" ],
    [ "Eric", "Role_I", "2017" ],
]

print(cooccurrences(performances1, 3))
print(cooccurrences(performances2, 3))
print(cooccurrences(performances3, 6))