from typing import *

# Definition for a binary tree node.
class TreeNode:
    def __init__(self, val=0, children=[]):
        self.val = val
        self.children = children

def rob(root: Optional[TreeNode]) -> int:
    def dfs(root: Optional[TreeNode]):
        if not root:
            return 0, 0
        selected, not_selected = root.val, 0
        for child in root.children:
            child_selected, child_not_selected = dfs(child)
            selected += child_not_selected
            not_selected += max(child_selected, child_not_selected)
        return selected, not_selected
    return max(dfs(root))

def func2(roots):
    def dfs(root):
        if not root:
            return 0, 0
        l_selected, l_not_selected = dfs(root.left)
        r_selected, r_not_selected = dfs(root.right)
        selected = root.val + l_not_selected + r_not_selected
        not_selected = max(l_selected, l_not_selected) + max(r_selected, r_not_selected)
        return selected, not_selected
    ret = 0
    for root in roots:
        ret += max(dfs(root))
    return ret

root = TreeNode(4, [TreeNode(3, [TreeNode(2), TreeNode(1), TreeNode(3)]), TreeNode(15, [TreeNode(2)])])
print(rob(root))