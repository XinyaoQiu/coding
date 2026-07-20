# TikTok — Coding Questions Prep

> Structure:
> - **Agent Notes**: internal rules for maintaining this file (for the agent, not interview content).
> - **Problems**: one entry per prepared problem. Each entry has three parts:
>   - **Problem** — the LeetCode-style problem statement.
>   - **Walkthrough** — a short spoken-style line: just what method/approach you'll use, plus the time and space complexity. Not a full walkthrough — code is the focus here.
>   - **Solution** — one clean, self-runnable code block: the solution plus an `if __name__ == "__main__"` block of `assert` tests you can run directly with `python`. Copy-paste and it runs.

---

## Agent Notes（内部规则，勿当面试内容 / 给 agent 读的，不是给用户读的）

生成/修改本文件时遵守：

1. **语言**：文件保存内容全英文；跟用户对话用中文。
2. **⚠️ TikTok coding 核心目标 —— 写出来 + 能自己跑 test（本文件与 amazon_coding.md 的核心区别）**：TikTok coding 面试基本是 LeetCode 题，最重要的是**当场把可运行的代码写出来，并能自己写 test case 跑通验证**。因此 solution 和 tests **合并成一个 code block**（不单列 Tests，避免函数体重复）：
   - **一个 code block 自包含可跑**：完整函数/类 + 必要 import + `if __name__ == "__main__"` 里的 `assert` 测试；复制出去 `python` 直接跑。
   - **tests 覆盖**：①正常例②边界（空/单元素/最大最小）③典型陷阱；最后 `print("all tests passed")` 方便确认。
   - **验证方式**：写完题目后用本地 Bash `python3` 跑一遍确认 AC 再入库（本地可直接跑，无需开发机）。
3. **三段结构**：每道题固定 **Problem → Walkthrough → Solution**（Solution 里含可跑 tests），顺序不变。
4. **Walkthrough 要求（⚠️ 本文件重代码，Walkthrough 极简）**：
   - 只需**简单说用什么方法**（如 "two-pointer" / "hash map for prefix sums" / "top-down DP with memo"），**再加时空复杂度**。一到两句即可，不做完整 UMPIRE，不讲暴力解、不讲 edge cases。
   - 口语化、第一人称（"I'd use…", "The idea is…"）；不含 markdown 格式、不含公式和代码；复杂度念大 O 英文（"linear time, constant space"），不写 `O(n)` 符号。
5. **Python solution 要求**：干净可跑的标准解法；注释最多一行、能不写就不写（沿用 CLAUDE.md 规范）；变量名可读。
6. **Tests 要求**：见第 2 条。测试要真能跑通对应 solution（函数名/签名一致）。
7. **收录范围**：只收用户明确指定要准备的题，不擅自加题。
8. **真实性**：解法以能 AC 为准，复杂度描述准确，不编造。

---

## Problems

> 占位框架。每道题按下面模板填写。

<!--
### Q1. <Problem name> (LeetCode <id>)

**Problem**

<statement>. Example 1: … Example 2: … Constraints: ….

**Walkthrough**

<一到两句：用什么方法 + 时空复杂度。无格式无代码>

**Solution**

```python
def solve(...):
    ...

if __name__ == "__main__":
    assert solve(...) == ...
    assert solve(...) == ...   # edge: empty / single / boundary
    print("all tests passed")
```
-->

### Q1. Most Frequent Element (tie → smaller)

**Problem**

Given an integer array, return the most frequent element. If there's a tie in frequency, return the smaller number. Example: `[1, 2, 2, 3, 3]` returns `2` (both 2 and 3 appear twice, pick the smaller). Constraints: the array is non-empty; values may be negative.

**Walkthrough**

I'd use a hash map to count each number's frequency, then do one pass over the counts, keeping the highest frequency and breaking ties by the smaller value. The key detail is the tie logic — I update when the frequency is strictly higher, or when it's equal but the value is smaller, and those two conditions must be separate, not combined with an and. Counting is linear and the scan is linear, so linear time and linear space.

**Solution**

```python
from collections import Counter


def most_frequent(nums):
    freq = Counter(nums)
    best_n, best_f = None, -1
    for n, f in freq.items():
        if f > best_f or (f == best_f and n < best_n):
            best_n, best_f = n, f
    return best_n


if __name__ == "__main__":
    assert most_frequent([1, 2, 2, 3, 3]) == 2      # tie 2 vs 3 -> smaller
    assert most_frequent([3, 3, 1]) == 3            # strict most frequent
    assert most_frequent([1, 2, 2]) == 2            # higher freq wins even if larger
    assert most_frequent([5]) == 5                  # single element
    assert most_frequent([4, 4, 4]) == 4            # all same
    assert most_frequent([-1, -1, 2, 2]) == -1      # negatives, tie -> smaller
    print("all tests passed")
```

---

### Q2. Min Operations to Equalize Root-to-Leaf Sums (N-ary tree)

**Problem**

Given an N-ary tree where each node has a value, in one operation you may increase a leaf's value by one. Return the minimum number of operations so that every root-to-leaf path has the same sum. Clarify with the interviewer that only leaves can be incremented and the operation is increment-only — the statement often omits this, and the examples imply it.

**Walkthrough**

Since I can only add to leaves, every root-to-leaf path has to be raised up to the largest one, so this is a post-order DFS, essentially tree DP. Each call returns the maximum root-to-leaf sum of its subtree; at every internal node I look at what its children return, take the max, and add up how far each child falls short of that max into a running total of operations, then return the node's own value plus that max so the parent can do the same. Doing it bottom-up like this, aligning each node's children locally, is equivalent to raising every path to the global maximum. It's one visit per node, so linear time, and the space is the recursion depth, which is the height of the tree.

**Solution**

```python
class TreeNode:
    def __init__(self, val=0, children=None):
        self.val = val
        self.children = children if children is not None else []


def min_ops(root):
    if not root:
        return 0
    ops = 0

    def dfs(node):
        nonlocal ops
        if not node.children:
            return node.val
        sums = [dfs(child) for child in node.children]
        m = max(sums)
        ops += sum(m - s for s in sums)
        return node.val + m

    dfs(root)
    return ops


if __name__ == "__main__":
    # paths 1+2=3, 1+3=4 -> max 4, ops = (4-3)+(4-4) = 1
    assert min_ops(TreeNode(1, [TreeNode(2), TreeNode(3)])) == 1
    assert min_ops(TreeNode(5)) == 0                                    # single node
    assert min_ops(TreeNode(1, [TreeNode(4), TreeNode(4)])) == 0        # already balanced
    # path sums 2,3,6 -> max 6, ops = 4+3+0 = 7
    assert min_ops(TreeNode(1, [TreeNode(1), TreeNode(2), TreeNode(5)])) == 7
    # nested: local align at inner node then at root -> total 1
    root = TreeNode(1, [TreeNode(2, [TreeNode(1), TreeNode(0)]), TreeNode(3)])
    assert min_ops(root) == 1
    print("all tests passed")
```

---

### Q3. Number of Islands (LeetCode 200)

**Problem**

Given an m by n grid of `'1'` (land) and `'0'` (water), return the number of islands. An island is land connected four-directionally (up, down, left, right) and surrounded by water. Example: a grid with one large connected landmass returns `1`; three separate landmasses return `3`.

**Walkthrough**

DFS flood-fill: scan the grid, and each time I hit unvisited land I bump the count and DFS the whole connected landmass, marking cells in place so I don't recount. Linear time in the number of cells; worst-case space is order m times n from the recursion depth when the whole grid is one island.

**Solution**

```python
def num_islands(grid):
    if not grid or not grid[0]:
        return 0
    m, n = len(grid), len(grid[0])
    dirs = [(0, -1), (-1, 0), (1, 0), (0, 1)]

    def dfs(i, j):
        grid[i][j] = '2'
        for di, dj in dirs:
            ni, nj = i + di, j + dj
            if 0 <= ni < m and 0 <= nj < n and grid[ni][nj] == '1':
                dfs(ni, nj)

    ans = 0
    for i in range(m):
        for j in range(n):
            if grid[i][j] == '1':
                ans += 1
                dfs(i, j)
    return ans


if __name__ == "__main__":
    g1 = [
        ["1", "1", "1", "1", "0"],
        ["1", "1", "0", "1", "0"],
        ["1", "1", "0", "0", "0"],
        ["0", "0", "0", "0", "0"],
    ]
    assert num_islands(g1) == 1
    g2 = [
        ["1", "1", "0", "0", "0"],
        ["1", "1", "0", "0", "0"],
        ["0", "0", "1", "0", "0"],
        ["0", "0", "0", "1", "1"],
    ]
    assert num_islands(g2) == 3
    assert num_islands([["0"]]) == 0                       # single water
    assert num_islands([["1"]]) == 1                       # single land
    assert num_islands([]) == 0                            # empty grid
    assert num_islands([["1", "0", "1", "0", "1"]]) == 3   # single row
    print("all tests passed")
```

**Follow-up A — Island areas and boundaries (variant)**

For each island, return a pair of its area (number of land cells) and its boundary — here defined as the count of distinct water cells it touches (out-of-grid edges not counted). Same DFS flood-fill: area comes up as the return value (one plus the children's), and adjacent water cells go into a per-island set so repeated touches count once. Linear time; space order m times n for the recursion stack plus the water set.

```python
def island_areas_and_boundaries(grid):
    if not grid or not grid[0]:
        return []
    m, n = len(grid), len(grid[0])
    dirs = [(0, -1), (-1, 0), (1, 0), (0, 1)]

    def dfs(i, j, neighbors):
        grid[i][j] = '2'
        area = 1
        for di, dj in dirs:
            ni, nj = i + di, j + dj
            if 0 <= ni < m and 0 <= nj < n:
                if grid[ni][nj] == '1':
                    area += dfs(ni, nj, neighbors)
                elif grid[ni][nj] == '0':
                    neighbors.add((ni, nj))
        return area

    ans = []
    for i in range(m):
        for j in range(n):
            if grid[i][j] == '1':
                neighbors = set()
                area = dfs(i, j, neighbors)
                ans.append([area, len(neighbors)])
    return ans


if __name__ == "__main__":
    assert island_areas_and_boundaries([["1", "0"], ["0", "0"]]) == [[1, 2]]
    assert island_areas_and_boundaries([["0", "0", "0"], ["0", "1", "0"], ["0", "0", "0"]]) == [[1, 4]]
    assert island_areas_and_boundaries([["1", "1"], ["1", "1"]]) == [[4, 0]]   # fills grid, no water
    assert island_areas_and_boundaries([["1", "1", "0", "1"], ["1", "0", "0", "0"]]) == [[3, 2], [1, 2]]
    assert island_areas_and_boundaries([["0"]]) == []
    assert island_areas_and_boundaries([]) == []
    print("all tests passed")
```

**Follow-up B — Number of Distinct Islands (LeetCode 694)**

Count islands with distinct shapes, where two are the same if one can be translated (not rotated/reflected) onto the other. DFS with a canonical signature: record each cell's offset relative to the island's first-visited cell and store the offset sequence in a set — the set size is the answer; a fixed direction order makes translation-equivalent islands produce identical signatures. Linear time; space order m times n.

```python
def num_distinct_islands(grid):
    if not grid or not grid[0]:
        return 0
    m, n = len(grid), len(grid[0])
    dirs = [(0, -1), (-1, 0), (1, 0), (0, 1)]
    shapes = set()

    def dfs(i, j, r0, c0, shape):
        grid[i][j] = 2
        shape.append((i - r0, j - c0))
        for di, dj in dirs:
            ni, nj = i + di, j + dj
            if 0 <= ni < m and 0 <= nj < n and grid[ni][nj] == 1:
                dfs(ni, nj, r0, c0, shape)

    for i in range(m):
        for j in range(n):
            if grid[i][j] == 1:
                shape = []
                dfs(i, j, i, j, shape)
                shapes.add(tuple(shape))
    return len(shapes)


if __name__ == "__main__":
    # two same-shaped 2x2 blocks -> 1 distinct
    assert num_distinct_islands([
        [1, 1, 0, 0, 0],
        [1, 1, 0, 0, 0],
        [0, 0, 0, 1, 1],
        [0, 0, 0, 1, 1],
    ]) == 1
    assert num_distinct_islands([[1, 0, 1, 1]]) == 2   # single cell vs horizontal pair
    assert num_distinct_islands([[1]]) == 1
    assert num_distinct_islands([[0]]) == 0
    assert num_distinct_islands([]) == 0
    print("all tests passed")
```

**Follow-up C — Number of Islands II (LeetCode 305): streaming addLand, or grid too big for memory**

Land is added one cell at a time and after each add we report the island count; also the answer if the grid is too large to hold in memory with sparse land. Union-find keyed by a dict of only the land cells: each new cell starts as its own component (count up one), then union with any of its four already-land neighbors (count down one per successful merge); a dict key doubles as the land-existence and out-of-bounds check. Near-constant time per operation with path compression, so order k times inverse-Ackermann overall; space proportional to the number of land cells, not the grid area — which is exactly what lets it scale past a grid that won't fit.

```python
def num_islands2(m, n, positions):
    parent = {}
    count = 0
    dirs = [(0, -1), (-1, 0), (1, 0), (0, 1)]

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]      # path compression
            x = parent[x]
        return x

    ans = []
    for r, c in positions:
        if (r, c) in parent:                   # duplicate addLand, count unchanged
            ans.append(count)
            continue
        parent[(r, c)] = (r, c)
        count += 1
        for dr, dc in dirs:
            nr, nc = r + dr, c + dc
            if (nr, nc) in parent:
                root1, root2 = find((r, c)), find((nr, nc))
                if root1 != root2:
                    parent[root1] = root2
                    count -= 1
        ans.append(count)
    return ans


if __name__ == "__main__":
    assert num_islands2(3, 3, [[0, 0], [0, 1], [1, 2], [2, 1]]) == [1, 1, 2, 3]
    assert num_islands2(1, 1, [[0, 0]]) == [1]
    assert num_islands2(3, 3, [[0, 0], [0, 0], [1, 1]]) == [1, 1, 2]   # duplicate coord
    print("all tests passed")
```

---

### Q4. Word Search II (LeetCode 212)

**Problem**

Given an m by n board of characters and a list of words, return all words in the list that can be formed by a path of horizontally or vertically adjacent cells, where each cell is used at most once per word. Example: on a 4x4 board with words `["oath","pea","eat","rain"]`, the answer is `["eat","oath"]`.

**Walkthrough**

Build a Trie from the word list, then DFS the board from every cell walking down the Trie in lockstep — prune the moment a character isn't in the current node, and collect the word when I hit an end marker. The Trie matches all words in one board traversal with shared prefixes instead of searching per word. Time is order cells times four times three to the max-word-length in the worst case; space is the total characters in the Trie plus the recursion depth.

**Solution**

```python
def find_words(board, words):
    if not board or not board[0]:
        return []
    trie = {}
    for w in words:
        node = trie
        for ch in w:
            node = node.setdefault(ch, {})
        node['$'] = w                         # end of word, store the word itself

    m, n = len(board), len(board[0])
    ans = []

    def dfs(i, j, node):
        ch = board[i][j]
        if ch not in node:                    # not in Trie -> prune
            return
        nxt = node[ch]
        if '$' in nxt:
            ans.append(nxt.pop('$'))          # collect, pop marker to dedupe
        board[i][j] = '#'                     # mark visited
        for di, dj in [(0, 1), (0, -1), (1, 0), (-1, 0)]:
            ni, nj = i + di, j + dj
            if 0 <= ni < m and 0 <= nj < n and board[ni][nj] != '#':
                dfs(ni, nj, nxt)
        board[i][j] = ch                      # restore

    for i in range(m):
        for j in range(n):
            dfs(i, j, trie)
    return ans


if __name__ == "__main__":
    board = [
        ["o", "a", "a", "n"],
        ["e", "t", "a", "e"],
        ["i", "h", "k", "r"],
        ["i", "f", "l", "v"],
    ]
    assert sorted(find_words(board, ["oath", "pea", "eat", "rain"])) == ["eat", "oath"]
    assert find_words([["a", "b"], ["c", "d"]], ["abcb"]) == []          # can't reuse cell
    assert sorted(find_words([["a", "b"]], ["a", "ab", "ba"])) == ["a", "ab", "ba"]
    assert find_words([["a"]], ["a"]) == ["a"]                           # single cell
    assert find_words([], ["a"]) == []                                   # empty board
    assert find_words([["a", "b"]], ["ab", "ab"]) == ["ab"]              # duplicate word
    print("all tests passed")
```
