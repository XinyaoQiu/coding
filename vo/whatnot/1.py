from typing import *

# singers = set()
# ans = {"Role_A": {"Role_B": 1}, "Role_B": {"Role_A": 1, "Role_C": 1}, ...}
# for each singer:
# roles = {"Role_A": 2010, "Role_B": 2013}
# roles_list = list(roles.keys())
# for i in range(..):
#      for j in range(i + 1, ..):
#           if abs <= period:
#               ans[x][y] += 1
#               ans[y][x] += 1                

def cooccurrences(performances: List[List[str]], period: int):
    singers = {}
    ans = {}
    for singer, role, year in performances:
        if singer not in singers:
            singers[singer] = {}
        if role not in ans:
            ans[role] = {}
        singers[singer][role] = int(year)
    for singer in singers.keys():
        roles_l = list(singers[singer].keys())
        for role in roles_l:
            if role not in ans:
                ans[role] = {}
        for i in range(len(roles_l)):
            for j in range(i + 1, len(roles_l)):
                x, y = roles_l[i], roles_l[j]
                if abs(singers[singer][x] - singers[singer][y]) <= period:
                    if y not in ans[x]:
                        ans[x][y] = 0
                    if x not in ans[y]:
                        ans[y][x] = 0
                    ans[x][y] += 1
                    ans[y][x] += 1
    return ans

    

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