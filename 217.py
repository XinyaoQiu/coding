from ref import *

class Solution:
    def containsDuplicate(self, nums: List[int]) -> bool:
        is_exist = {}
        for num in nums:
            if num in is_exist:
                return True
            else:
                is_exist[num] = 0
        return False
    
    