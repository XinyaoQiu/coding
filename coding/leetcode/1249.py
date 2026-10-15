from typing import *

class Solution:
    def minRemoveToMakeValid(self, s: str) -> str:
        stack = []
        to_delete = set()
        for i in range(len(s)):
            if s[i] == '(':
                stack.append(i)
            elif s[i] == ')':
                if len(stack) == 0:
                    to_delete.add(i)
                else:
                    stack.pop()
        to_delete |= set(stack)
        ans = ""
        for i in range(len(s)):
            if i not in to_delete:
                ans += s[i]
        return ans