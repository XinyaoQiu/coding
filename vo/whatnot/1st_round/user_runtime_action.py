from collections import defaultdict

records = [
    "100 1000 A",
    "200 1100 A",
    "100 1200 B",
    "200 1200 B",
    "100 1300 C",
    "200 1400 A",
    "300 1500 B",
    "300 1550 B",
    "100 1600 D",
]

# {100: [A, B, C, D], 200: [A, B, A], 300: [B, B]}
# 100: A 1 B 1 C 1 D 1
# 200: A 1 B 1 A 1

"""
            0
          /A \B
         2    1
        /B    \B
       2       1
      /C \A
     1    1
    /D    
   1       
"""

"""
A 2
    ->B 2
        ->C 1
        ->A 1
B 1
    ->B 1
"""

class TrieNode:
    def __init__(self):
        self.count = 0
        self.children = defaultdict(TrieNode)

user_paths = defaultdict(list)
for record in records:
    user, _, action = record.split()
    user_paths[user].append(action)

root = TrieNode()
for user, actions in user_paths.items():
    node = root
    for action in actions:
        node = node.children[action]
        node.count += 1

def print_trie(node, indent=0):
    for action, child in node.children.items():
        prefix = "  " * indent + ("->" if indent else "") + f"{action} {child.count}"
        print(prefix)
        print_trie(child, indent + 1)

print_trie(root)
