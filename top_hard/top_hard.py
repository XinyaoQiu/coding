import heapq
from collections import defaultdict, deque
from typing import *

# leetcode 42
def trap(height):
    left, right = 0, len(height) - 1
    left_max = right_max = 0
    ans = 0
    while left <= right:
        if left_max <= right_max:
            ans += max(left_max - height[left], 0)
            left_max = max(left_max, height[left])
            left += 1
        else:
            ans += max(right_max - height[right], 0)
            right_max = max(right_max, height[right])
            right -= 1
    return ans

# leetcode 84
def largestRectangleArea(heights):
    stack = [-1]
    ans = 0
    for i, h in enumerate(heights + [0]):
        while stack[-1] != -1 and heights[stack[-1]] >= h:
            curr = heights[stack.pop()]
            ans = max(ans, curr * (i - stack[-1] - 1))
        stack.append(i)
    return ans

# leetcode 85
def maximalRectangle(matrix):
    m, n = len(matrix), len(matrix[0])
    heights = [0] * n
    ans = 0
    for i in range(m):
        stack = [-1]
        for j in range(n):
            if matrix[i][j] == "1":
                heights[j] += 1
            else:
                heights[j] = 0
        for j, h in enumerate(heights + [0]):
            while stack[-1] != -1 and heights[stack[-1]] >= h:
                curr = heights[stack.pop()]
                ans = max(ans, curr * (j - stack[-1] - 1))
            stack.append(j)
    return ans

# leetcode 
class MedianFinder:
    def __init__(self):
        self.left = [] # max heap
        self.right = [] # min heap

    def addNum(self, num: int) -> None:
        if len(self.left) == len(self.right):
            heapq.heappush(self.left, -heapq.heappushpop(self.right, num))
        else:
            heapq.heappush(self.right, -heapq.heappushpop(self.left, -num))


    def findMedian(self) -> float:
        if len(self.left) == len(self.right):
            return (-self.left[0] + self.right[0]) / 2.0
        return -self.left[0]


# leetcode 355
class Twitter:

    def __init__(self):
        self.timer = 0
        self.tweets = defaultdict(deque)
        self.followees = defaultdict(set)
        self.maxRecent = 10

    def postTweet(self, userId: int, tweetId: int) -> None:
        dq = self.tweets[userId]
        dq.appendleft((self.timer, tweetId))
        if len(dq) > self.maxRecent:
            dq.pop()
        self.timer += 1

    def getNewsFeed(self, userId: int) -> List[int]:
        users = self.followees[userId] | {userId}
        heap = []
        for uid in users:
            dq = self.tweets[uid]
            if dq:
                heapq.heappush(heap, (-dq[0][0], dq[0][1], uid, 0))
        res = []
        while heap and len(res) < self.maxRecent:
            _, tid, uid, idx = heapq.heappop(heap)
            res.append(tid)
            dq = self.tweets[uid]
            if idx + 1 < len(dq):
                nxt = dq[idx + 1]
                heapq.heappush(heap, (-nxt[0], nxt[1], uid, idx + 1))
        return res

    def follow(self, followerId: int, followeeId: int) -> None:
        self.followees[followerId].add(followeeId)

    def unfollow(self, followerId: int, followeeId: int) -> None:
        self.followees[followerId].discard(followeeId)


# leetcode 23
class ListNode:
    def __init__(self, val=0, next=None):
        self.val = val
        self.next = next

    def __lt__(self, other):
        return self.val < other.val

def mergeKLists(lists):
    def merge(l1, l2):
        dummy = ListNode()
        curr = dummy
        while l1 and l2:
            if l1.val < l2.val:
                curr.next = l1
                l1 = l1.next
            else:
                curr.next = l2
                l2 = l2.next
            curr = curr.next
        curr.next = l1 or l2
        return dummy.next

    def mergeLists(L, R):
        if L > R:
            return None
        if L == R:
            return lists[L]
        M = (L + R) // 2
        return merge(mergeLists(L, M), mergeLists(M + 1, R))

    return mergeLists(0, len(lists) - 1)

def mergeKLists(lists):
    heap = [node for node in lists if node]
    dummy = ListNode()
    curr = dummy
    while heap:
        node = heapq.heappop(heap)
        curr.next = node
        curr = curr.next
        if node.next:
            heapq.heappush(heap, node.next)
    return dummy.next

# leetcode 460
class LFUCache:
    def __init__(self, capacity: int):
        self.capacity = capacity
        self.freq_map = defaultdict(OrderedDict)
        self.key_map = {}
        self.min_freq = 0

    def _touch(self, key: int) -> int:
        val, freq = self.key_map[key]
        del self.freq_map[freq][key]
        if not self.freq_map[freq]:
            del self.freq_map[freq]
            if self.min_freq == freq:
                self.min_freq += 1
        self.freq_map[freq + 1][key] = None
        self.freq_map[freq + 1].move_to_end(key, last=False)
        self.key_map[key] = (val, freq + 1)
        return val

    def get(self, key: int) -> int:
        if key not in self.key_map:
            return -1
        return self._touch(key)

    def put(self, key: int, value: int) -> None:
        if self.capacity == 0:
            return
        if key in self.key_map:
            self._touch(key)
            _, freq = self.key_map[key]
            self.key_map[key] = (value, freq)
            return
        if len(self.key_map) == self.capacity:
            evict_key, _ = self.freq_map[self.min_freq].popitem()
            if not self.freq_map[self.min_freq]:
                del self.freq_map[self.min_freq]
            del self.key_map[evict_key]
        self.key_map[key] = (value, 1)
        self.freq_map[1][key] = None
        self.freq_map[1].move_to_end(key, last=False)
        self.min_freq = 1

# leetcode 305
def number_of_islands_2(m, n, positions):
    parent = {}
    size = {}
    count = 0

    def find(x):
        while x != parent[x]:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x
    def union(x, y):
        nonlocal count
        rx, ry = find(x), find(y)
        if rx == ry:
            return
        if size[rx] > size[ry]:
            rx, ry = ry, rx
        parent[rx] = ry
        size[ry] += size[rx]
        count -= 1

    dirs = [(0, -1), (-1, 0), (0, 1), (1, 0)]
    ans = []
    for r, c in positions:
        if (r, c) in parent:
            ans.append(count)
            continue
        count += 1
        parent[(r, c)] = (r, c)
        size[(r, c)] = 1
        for dr, dc in dirs:
            nr, nc = r + dr, c + dc
            if 0 <= nr < m and 0 <= nc < n and (nr, nc) in parent:
                union((r, c), (nr, nc))
        ans.append(count)

    return ans

# leetcode 72
def edit_distance(word1, word2):
    if len(word1) < len(word2):
        word1, word2 = word2, word1
    m, n = len(word1), len(word2)
    dp = [0] + [j + 1 for j in range(n)]
    prev = 0
    for i in range(m):
        prev = dp[0]
        dp[0] = i + 1
        for j in range(1, n + 1):
            tmp = dp[j]
            prev += 0 if word1[i] == word2[j - 1] else 1
            dp[j] = min([prev, dp[j] + 1, dp[j - 1] + 1])
            prev = tmp
    return dp[n]

# leetcode 312
def solution(nums):
    nums = [1] + nums + [1]
    memo = {}
    def dfs(i, j):
        if j == i + 1:
            return 0
        if (i, j) in memo:
            return memo[(i, j)]
        ans = max(dfs(i, k) + dfs(k, j) + nums[i] * nums[k] * nums[j] for k in range(i + 1, j))
        memo[(i, j)] = ans
        return ans
    return dfs(0, len(nums) - 1)

# leetcode 39
def solution(candidates, target):
    ans = []
    def dfs(i, t, path):
        if t < 0:
            return
        if t == 0:
            ans.append(path.copy())
            return
        for j in range(i, len(candidates)):
            path.append(candidates[j])
            dfs(j, t - candidates[j], path)
            path.pop()
    dfs(0, target, [])
    return ans

# leetcode 40
def solution(candidates, target):
    ans = []
    path = []
    n = len(candidates)
    candidates.sort()
    def dfs(i, t):
        if t == 0:
            ans.append(path.copy())
            return
        if i == n or t < 0:
            return
        path.append(candidates[i])
        dfs(i + 1, t - candidates[i])
        path.pop()
        i += 1
        while i < n and candidates[i] == candidates[i - 1]:
            i += 1
        dfs(i, t)
    dfs(0, target)
    return ans

# leetcode 46
def permute(nums):
    ans = []
    path = []
    n = len(nums)
    used = [False] * n
    def dfs():
        if len(path) == n:
            ans.append(path.copy())
            return
        for j in range(n):
            if not used[j]:
                used[j] = True
                path.append(nums[j])
                dfs()
                path.pop()
                used[j] = False
    dfs()
    return ans

# leetcode 47
def permute(nums):
    ans = []
    path = []
    n = len(nums)
    nums.sort()
    used = [False] * n
    def dfs():
        if len(path) == n:
            ans.append(path.copy())
            return
        for i in range(n):
            if i > 0 and nums[i] == nums[i - 1] and not used[i - 1]:
                continue
            if not used[i]:
                used[i] = True
                path.append(nums[i])
                dfs()
                used[i] = False
                path.pop()
    dfs()
    return ans

def permute(nums):
    ans = []
    path = []
    n = len(nums)
    used = [False] * n
    def dfs():
        if len(path) == n:
            ans.append(path.copy())
            return
        seen = set()
        for i in range(n):
            if used[i] or seen[nums[i]]:
                continue
            path.append(nums[i])
            used[i] = True
            seen.add(nums[i])
            dfs()
            path.pop()
            used[i] = False
    dfs()
    return ans

# leetcode 269
def alien_dictionary(words):
    chars = set(c for w in words for c in w)
    edges = defaultdict(list)
    indegs = defaultdict(int)
    for i in range(len(words) - 1):
        a, b = words[i], words[i+1]
        if len(a) > len(b) and a.startswith(b):
            return ""
        for u, v in zip(a, b):
            if u != v:
                edges[u].append(v)
                indegs[v] += 1
                break
    q = deque(c for c in chars if indegs[c] == 0)
    ans = []
    while q:
        u = q.popleft()
        ans.append(u)
        for v in edges[u]:
            indegs[v] -= 1
            if indegs[v] == 0:
                q.append(v)
    return "".join(ans) if len(ans) == len(chars) else ""

# leetcode 79
def word_search(board, word):
    m, n = len(board), len(board[0])
    dirs = [(0, -1), (-1, 0), (0, 1), (1, 0)]

    def dfs(r, c, i):
        if board[r][c] != word[i]:
            return False
        if i == len(word) - 1:
            return True
        board[r][c] = "#"
        for dr, dc in dirs:
            nr, nc = r + dr, c + dc
            if 0 <= nr < m and 0 <= nc < n and dfs(nr, nc, i + 1):
                board[r][c] = word[i]
                return True
        board[r][c] = word[i]
        return False

    return any(dfs(r, c, 0) for r in range(m) for c in range(n))

# word search + can duplicate
def word_search(board, word):
    m, n = len(board), len(board[0])
    dirs = [(0, -1), (-1, 0), (0, 1), (1, 0)]
    memo = {}

    def dfs(r, c, i):
        if board[r][c] != word[i]:
            return False
        if i == len(word) - 1:
            return True
        if (r, c, i) in memo:
            return memo[(r, c, i)]
        for dr, dc in dirs:
            nr, nc = r + dr, c + dc
            if 0 <= nr < m and 0 <= nc < n and dfs(nr, nc, i + 1):
                return True
        memo[(r, c, i)] = False
        return False

    return any(dfs(r, c, 0) for r in range(m) for c in range(n))

# leetcode 212
class TrieNode:
    def __init__(self):
        self.children = {}
        self.end = ""

def word_search_2(board, words):
    root = TrieNode()
    for word in words:
        curr = root
        for ch in word:
            if ch not in curr.children:
                curr.children[ch] = TrieNode()
            curr = curr.children[ch]
        curr.end = word
    m, n = len(board), len(board[0])
    dirs = [(0, -1), (-1, 0), (0, 1), (1, 0)]
    ans = []

    def dfs(r, c, node):
        ch = board[r][c]
        if ch not in node.children:
            return
        nxt = node.children[ch]
        if nxt.end:
            ans.append(nxt.end)
            nxt.end = ""
        board[r][c] = "#"
        for dr, dc in dirs:
            nr, nc = r + dr, c + dc
            if 0 <= nr < m and 0 <= nc < n:
                dfs(nr, nc, nxt)
        board[r][c] = ch
        if not nxt.children and not nxt.end:
            del node.children[ch]

    for r in range(m):
        for c in range(n):
            dfs(r, c, root)

    return ans

def word_search_2(board, words):
    root = TrieNode()
    for word in words:
        curr = root
        for ch in word:
            if ch not in curr.children:
                curr.children[ch] = TrieNode()
            curr = curr.children[ch]
        curr.end = word
    m, n = len(board), len(board[0])
    dirs = [(0, -1), (-1, 0), (0, 1), (1, 0)]
    ans = []

    def dfs(r, c, node):
        ch = board[r][c]
        if ch not in node.children:
            return
        nxt = node.children[ch]
        if nxt.end:
            ans.append(nxt.end)
            nxt.end = ""
        for dr, dc in dirs:
            nr, nc = r + dr, c + dc
            if 0 <= nr < m and 0 <= nc < n:
                dfs(nr, nc, nxt)
        if not nxt.children and not nxt.end:
            del node.children[ch]

    for r in range(m):
        for c in range(n):
            dfs(r, c, root)


# leetcode 22
def solution(n):
    ans = []
    path = []

    def dfs(left, right):
        if left == n and right == n:
            ans.append("".join(path))
            return
        if left < n:
            path.append('(')
            dfs(left + 1, right)
            path.pop()
        if right < left:
            path.append(')')
            dfs(left, right + 1)
            path.pop()

    dfs(0, 0)
    return ans

# leetcode 301
def solution(s):
    leftmove = rightmove = 0
    for ch in s:
        if ch == '(':
            leftmove += 1
        elif ch == ')':
            if leftmove > 0:
                leftmove -= 1
            else:
                rightmove += 1

    def is_valid(a):
        count = 0
        for ch in a:
            if ch == '(':
                count += 1
            elif ch == ')':
                if count == 0:
                    return False
                count -= 1
        return count == 0

    ans = []
    def dfs(cur, i, left, right):
        if left == 0 and right == 0:
            if is_valid(cur):
                ans.append(cur)
            return
        for j in range(i, len(cur)):
            if j > i and cur[j] == cur[j-1]:
                continue
            if left > 0 and cur[j] == '(':
                dfs(cur[:j] + cur[j+1:], j, left - 1, right)
            if right > 0 and cur[j] == ')':
                dfs(cur[:j] + cur[j+1:], j, left, right - 1)
    return ans

def solution(s):
    def is_valid(a):
        count = 0
        for ch in a:
            if ch == '(':
                count += 1
            elif ch == ')':
                if count == 0:
                    return False
                count -= 1
        return count == 0

    q = deque([s])
    ans = []
    while q:
        visited = set()
        for _ in range(len(q)):
            x = q.popleft()
            if is_valid(x):
                ans.append(x)
                continue
            for i in range(len(x)):
                if i > 0 and x[i] == x[i-1]:
                    continue
                if x[i] == '(' or x[i] == ')':
                    y = x[:i] + x[i+1:]
                    if y not in visited:
                        visited.add(y)
                        q.append(y)
        if len(ans) > 0:
            break
    return ans

# leetcode 32
def solution(s):
    stack = [-1]
    ans = 0
    for i, ch in enumerate(s):
        if ch == '(':
            stack.append(i)
        else:
            if len(stack) == 1:
                stack[0] = i
            else:
                stack.pop()
                ans = max(ans, i - stack[-1])
    return ans
    
# leetcode 239
def solution(nums, k):
    q = deque()
    ans = []
    for i, num in enumerate(nums):
        if i >= k and q and q[0] == i - k:
            q.popleft()
        while q and nums[q[-1]] <= num:
            q.pop()
        q.append(i)
        if i >= k - 1:
            ans.append(nums(q[0]))
    return ans

# leetcode 560
def solution(nums, k):
    mp = defaultdict(int)
    mp[0] = 1
    run = 0
    ans = 0
    for n in nums:
        run += n
        ans += mp[run - k]
        mp[run] += 1
    return ans

def solution(grid, k):
    m, n = len(grid), len(grid[0])
    dirs = [(0, -1), (-1, 0), (0, 1), (1, 0)]
    q = deque([(0, 0, 0, 0)])
    visited = set([(0, 0, 0)])
    while q:
        i, j, used, step = q.popleft()
        if (i, j) == (m - 1, n - 1):
            return step
        for di, dj in dirs:
            ni, nj = i + di, j + dj
            if 0 <= ni < m and 0 <= nj < n:
                nused = used + grid[ni][nj]
                if nused <= k and (ni, nj, nused) not in visited:
                    visited.add((ni, nj, nused))
                    q.append((ni, nj, nused, step + 1))
    return -1        