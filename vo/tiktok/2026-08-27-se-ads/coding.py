"""
nums[i] < nums[i+1]
x < a >= b >= c
c <= x
b > x
b <= a > x >= c
b, c, x, a is next of x, a, b, c
"""

# leetcode 31 - Next Permutation
def solution(nums):
    n = len(nums)
    i = n - 2
    while i >= 0 and nums[i] >= nums[i+1]:
        i -= 1
    if i >= 0:
        j = n - 1
        while j >= i and nums[j] <= nums[i]:
            j -= 1
        nums[i], nums[j] = nums[j], nums[i]
    left, right = i + 1, n - 1
    while left < right:
        nums[left], nums[right] = nums[right], nums[left]
        left += 1
        right -= 1
    

# leetcode 556 - Next Greater Element III
def solution(n):
    digits = list(str(n))
    l = len(digits)
    pivot = -1
    for i in range(l - 2, -1, -1):
        if digits[i] < digits[i+1]:
            pivot = i
            break
    if pivot == -1:
        return -1
    for i in range(l - 1, pivot, -1):
        if digits[i] > digits[pivot]:
            digits[pivot], digits[i] = digits[i], digits[pivot]
            break
    ans = int(''.join(digits[:pivot+1] + list(reversed(digits[pivot+1:]))))
    return ans if ans < 2 ** 31 else -1
    
# leetcode 238 - Product of Array Except Self
def solution(nums):
    ans = []
    prod = 1
    for x in nums:
        ans.append(prod)
        prod *= x
    prod = 1
    for i in range(len(nums) - 1, -1, -1):
        ans[i] *= prod
        prod *= nums[i]
    return ans

# leetcode 287 - Find the Duplicate Number
def solution(nums):
    slow = fast = 0
    while 1:
        slow = nums[slow]
        fast = nums[nums[fast]]
        if slow == fast:
            break
    slow = 0
    while slow != fast:
        slow = nums[slow]
        fast = nums[nums[fast]]
    return slow

def solution(nums):
    while nums[0] != nums[nums[0]]:
        nums[nums[0]], nums[0] = nums[0], nums[nums[0]]
    return nums[0]

# leetcode 128 - Longest Consecutive Sequence
def solution(nums):
    nums = set(nums)
    ans = 0
    for x in nums:
        if x - 1 not in nums:
            y = x + 1
            while y in nums:
                y += 1
            ans = max(ans, y - x)
    return ans

"""
Same cyclic-sort / index-as-value family:
41 First Missing Positive · 442 Find All Duplicates in an Array · 448 Find All Numbers Disappeared in an Array

In-place array manipulation:
189 Rotate Array · 73 Set Matrix Zeroes · 283 Move Zeroes · 169 Majority Element

Prefix/suffix and two pointers:
42 Trapping Rain Water · 11 Container With Most Water · 152 Maximum Product Subarray

Hash-based O(n):
560 Subarray Sum Equals K · 49 Group Anagrams

String:
76 Minimum Window Substring · 3 Longest Substring Without Repeating Characters · 5 Longest Palindromic Substring

Intervals, since round 1 opened with one:
56 Merge Intervals · 57 Insert Interval · 435 Non-overlapping Intervals · 1094 Car Pooling
"""

# leetcode 41
def firstMissingPositive(nums):
    n = len(nums)
    for i in range(n):
        while 1 <= nums[i] <= n and nums[i] != nums[nums[i] - 1]:
            nums[nums[i] - 1], nums[i] = nums[i], nums[nums[i] - 1]
    for i in range(n):
        if i != nums[i] - 1:
            return i + 1
    return n + 1

# leetcode 442
def findDuplicates(nums):
    n = len(nums)
    for i in range(n):
        while nums[i] != nums[nums[i] - 1]:
            nums[nums[i] - 1], nums[i] = nums[i], nums[nums[i] - 1]
    return [num for i, num in enumerate(nums) if i != num - 1]

def findDuplicates(nums):
    ans = []
    for x in nums:
        x = abs(x)
        if nums[x - 1] > 0:
            nums[x - 1] = -nums[x - 1]
        else:
            ans.append(x)
    return ans