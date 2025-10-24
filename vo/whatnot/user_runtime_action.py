from collections import defaultdict

records = [
    ['100', '1000', 'A'],
    ['200', '1100', 'A'],
    ['100', '1200', 'B'],
    ['200', '1200', 'B'],
    ['100', '1300', 'C'],
    ['200', '1400', 'A'],
    ['300', '1500', 'B'],
    ['300', '1550', 'B'],
]

# {100: [A, B, C], 200: [A, B, A], 300: [B, B]}
# {(A,): 2, (A, B): 2, (A, B, C): 1, (A, B, A): 1, (B,): 1, (B, B): 1}

"""
A 2
    B 2
        C 1
        A 1
B 1
    B 1
"""


user_path = defaultdict(list)
for user, time, action in records:
    user_path[user].append((int(time), action))
for user in user_path.keys():
    user_path[user].sort()
counts = defaultdict(int)
for user, path in user_path.items():
    path = [x[1] for x in path]
    for i in range(len(path)):
        counts[tuple(path[:i+1])] += 1
print(dict(counts))

def dfs_print(prefix=(), indent=0):
    nexts = [x for x in counts if x[:-1] == prefix]
    for next in nexts:
        print("    " * indent + f"{next[-1]} {counts[next]}")
        dfs_print(next, indent + 1)

dfs_print()


    
        