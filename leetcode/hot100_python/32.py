from typing import *

class Solution:
    def longestValidParentheses(self, s: str) -> int:
        if s == "":
            return 0
        n = len(s)
        dp = [0] * (n + 1)
        for i in range(1, n):
            if s[i] == ')' and s[i - 1] == '(':
                dp[i] = dp[i - 2] + 2
            elif s[i] == ')' and s[i - 1] == ')':
                j = i - 1 - dp[i - 1]
                if j >= 0 and s[j] == '(':
                    dp[i] = dp[j - 1] + dp[i - 1] + 2
        return max(dp)

