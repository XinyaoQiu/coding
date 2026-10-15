from typing import *
from collections import defaultdict

class TreeNode:
    def __init__(self, val=0, left=None, right=None):
        self.val = val
        self.left = left
        self.right = right

class Solution:
    def pathSum(self, root: Optional[TreeNode], targetSum: int) -> int:
        if not root:
            return 0
        mp = defaultdict(int)
        mp[0] = 1
        def dfs(root: Optional[TreeNode], currSum: int) -> int:
            if not root:
                return 0
            currSum += root.val
            ans = mp[currSum - targetSum]
            mp[currSum] += 1
            ans += dfs(root.left, currSum) + dfs(root.right, currSum)
            mp[currSum] -= 1
            return ans
        return dfs(root, 0)