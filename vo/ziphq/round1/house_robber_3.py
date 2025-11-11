from typing import *

# Definition for a binary tree node.
class TreeNode:
    def __init__(self, val=0, left=None, right=None):
        self.val = val
        self.left = left
        self.right = right

class Solution:
    def rob(self, root: Optional[TreeNode]) -> int:
        def dfs(root):
            if not root:
                return 0, 0
            l_selected, l_not_selected = dfs(root.left)
            r_selected, r_not_selected = dfs(root.right)
            selected = root.val + l_not_selected + r_not_selected
            not_selected = max(l_selected, l_not_selected) + max(r_selected, r_not_selected)
            return selected, not_selected
        return max(dfs(root))