# Amazon SDE I — Coding Questions Prep

> Structure:
> - **Agent Notes**: internal rules for maintaining this file (for the agent, not interview content).
> - **Problems**: one entry per prepared problem. Each entry has three parts:
>   - **Problem** — the LeetCode-style problem statement.
>   - **Walkthrough** — a spoken-style English explanation, the way you'd talk the interviewer through the problem live. Plain conversational English, no formatting, no formulas, no code — describe the approach, the data structure, the transition, and the complexity all in words.
>   - **Python solution** — the actual code.

---

## Agent Notes（内部规则，勿当面试内容 / 给 agent 读的，不是给用户读的）

生成/修改本文件时遵守：

1. **语言**：文件保存内容全英文；跟用户对话用中文。
2. **三段结构**：每道题固定 **Problem → Walkthrough → Python solution** 三部分，顺序不变。
3. **Walkthrough 要求**：
   - 口语化，像面试时对着面试官讲，不是书面文档。用第一人称（"I'd start by…", "The idea is…", "So the way I think about it…"）。
   - **不含任何格式**：不用 markdown 列表、加粗、标题、表格。就是几段连续的自然语言。
   - **不含公式和代码**：状态转移、递推、复杂度全部用**英语口语描述**。比如不写 `dp[i]=dp[i-1]+dp[i-2]`，而是说 "each state is the sum of the previous two"。复杂度可以口语念大 O（"O of n squared" / "linear" / "quadratic"），但**不写符号 `O(n^2)`**，写成英文读法或用词描述。
   - **结构按 UMPIRE 模板**（面试通用框架，来源 CodePath），四段顺序固定：
     1. **Restate + constraints/clarify** — 复述题意，问清约束和边界（1 句，如 "First thing I'd clarify is whether duplicates are allowed"）。
     2. **Brute force + its time complexity** — 先给最朴素解法和它的复杂度，证明理解了问题；顺带点出朴素解法的陷阱（1-2 句）。
     3. **Optimal approach + why + time/space complexity** — 关键洞察 → 最优解思路 → 时间和空间复杂度（主体段）。
     4. **Edge cases**（可选）— 收尾提一句坑或边界。
   - 长度适中：能讲清楚即可，通常每段 2-4 句、四段合计两三段的量。别写成论文。
4. **Python solution 要求**：干净可跑的标准解法；注释最多一行、能不写就不写（沿用项目 CLAUDE.md 规范）；变量名可读。
5. **收录范围**：只收用户明确指定要准备的题，不擅自加题。
6. **真实性**：解法以能 AC 为准，复杂度描述准确，不编造。

---

## Problems

### Q1. Validate Binary Search Tree (LeetCode 98)

**Problem**

Given the `root` of a binary tree, determine if it is a valid binary search tree (BST). A valid BST is defined as: the left subtree of a node contains only nodes with keys strictly less than the node's key; the right subtree contains only nodes with keys strictly greater than the node's key; and both subtrees are themselves BSTs.

Example 1: `root = [2,1,3]` returns `true`. Example 2: `root = [5,1,4,null,null,3,6]` returns `false`, because the root is 5 but its right child is 4.

Constraints: number of nodes is in the range one to ten thousand, and each node value fits in a 32-bit signed integer.

**Walkthrough**

So this is a binary tree and I need to check the BST property. First thing I'd clarify is whether duplicates are allowed. Here the rule is strictly less on the left and strictly greater on the right, so equal values are invalid.

The naive idea is, at every node, check that everything in its left subtree is smaller and everything in the right is larger. That works but it's wasteful, because for each node you end up scanning its whole subtree, so it's quadratic in the worst case. And the classic bug people fall into is only comparing a node against its direct parent, which misses violations against deeper ancestors.

The optimal approach is a single top-down pass where I carry a valid range for each node, a lower bound and an upper bound. The root is unbounded. Going left tightens the upper bound down to the current node's value, and going right tightens the lower bound up to it. Each node just has to sit strictly inside the range it was handed, and because the bounds only get tighter going down, this naturally enforces the ancestor constraint. That's one visit per node, so linear time, and the space is the height of the tree from the recursion stack.

One edge detail is that the comparison has to be strict since equal values aren't allowed, and starting the bounds at negative and positive infinity keeps the values at the integer limits from being wrongly rejected.

**Python solution**

```python
def isValidBST(root):
    def valid(node, low, high):
        if not node:
            return True
        if not (low < node.val < high):
            return False
        return valid(node.left, low, node.val) and valid(node.right, node.val, high)
    return valid(root, float('-inf'), float('inf'))
```

---

### Q2. Best Time to Buy and Sell Stock (LeetCode 121)

**Problem**

You are given an array `prices` where `prices[i]` is the price of a given stock on the i-th day. You want to maximize your profit by choosing a single day to buy one stock and choosing a different day in the future to sell that stock. Return the maximum profit you can achieve from this transaction. If you cannot achieve any profit, return zero.

Example 1: `prices = [7,1,5,3,6,4]` returns `5`, buying at 1 on day two and selling at 6 on day five. Example 2: `prices = [7,6,4,3,1]` returns `0`, since prices only fall.

Constraints: array length from one up to about ten to the fifth, and prices from zero up to about ten to the fourth.

**Walkthrough**

So I'm given daily prices and I want to buy once and sell once on a later day to maximize profit. First thing I'd clarify is that the sell day has to be strictly after the buy day, and if there's no profitable pair I return zero.

The brute force is to try every pair of days, a buy day and a later sell day, and take the best difference. That's all pairs, so it's quadratic, too slow at the upper end.

There are two ways I'd present the optimal solution. The direct one is a single left-to-right scan: as I stand on each day as a potential sell day, the best I can do is today's price minus the cheapest price I've seen so far, so I just keep a running minimum and update my best answer each day. That's linear time and constant space.

But I'd frame it in the way that generalizes to every follow-up, which is a two-state machine. On each day I'm in exactly one of two states, and I track the best money I can have in each. The first state, I call it `cash`, is the most money I have while holding no share. The second, `hold`, is the most money I have while currently holding one share; since buying costs money, `hold` is typically negative. Each day, `hold` is the better of staying in hold or buying today, and `cash` is the better of staying in cash or selling today's share. The answer is `cash` at the end, because finishing while still holding is never better than having sold.

The one subtlety worth explaining is the initial values. `cash` starts at zero, since before doing anything I have no share and no profit. `hold` starts at negative infinity, because on day zero it's impossible to already be holding a share, and negative infinity is a sentinel meaning this state is unreachable so far. The first time I actually buy, `hold` becomes a real number and negative infinity gets overwritten. For this single-transaction version, buying always starts fresh from zero rather than from accumulated cash, which is what limits it to one transaction.

**Python solution**

Running-minimum version (most direct):

```python
def maxProfit(prices):
    min_price = float('inf')
    best = 0
    for p in prices:
        best = max(best, p - min_price)
        min_price = min(min_price, p)
    return best
```

State-machine version (generalizes to the follow-ups):

```python
def maxProfit(prices):
    hold, cash = float('-inf'), 0
    for p in prices:
        hold = max(hold, -p)
        cash = max(cash, hold + p)
    return cash
```

**Follow-up 1 — Unlimited transactions (LeetCode 122)**

Now I can make as many transactions as I want, still holding at most one share at a time. The state machine barely changes. The only difference is what money I can reuse when I buy: with one transaction, buying always started fresh from zero, but now I can put profit I've already banked back in, so buying draws from my current `cash`. That single change from `minus p` to `cash minus p` is the whole difference between one transaction and unlimited. There's also a neat greedy shortcut for this specific case: since I can trade freely, the total profit equals the sum of every day-over-day rise, so I can just add up each positive step and skip the drops. Both are linear time and constant space.

```python
def maxProfit(prices):
    hold, cash = float('-inf'), 0
    for p in prices:
        hold = max(hold, cash - p)
        cash = max(cash, hold + p)
    return cash
```

**Follow-up 2 — One function for both single and unlimited (LeetCode 121 and 122)**

The interviewer may ask to cover both cases with one function. Since the only difference is whether buying reuses accumulated cash or starts from zero, I gate that one term with a flag. With the flag off it's the single-transaction case, with it on it's unlimited. Everything else, the two states and the linear scan, is identical.

```python
def maxProfit(prices, unlimited=True):
    hold, cash = float('-inf'), 0
    for p in prices:
        base = cash if unlimited else 0
        hold = max(hold, base - p)
        cash = max(cash, hold + p)
    return cash
```

**Follow-up 3 — Unlimited transactions with a transaction fee (LeetCode 714)**

Now every completed transaction charges a fixed fee. The greedy shortcut breaks, because blindly capturing every small rise would lose the fee each time, so I stay with the state machine. The only change from the unlimited case is that I subtract the fee once per completed transaction, and I do it at the moment I sell. Charging it only on sell guarantees I pay exactly once per round trip. Still linear time and constant space.

```python
def maxProfit(prices, fee):
    hold, cash = float('-inf'), 0
    for p in prices:
        hold = max(hold, cash - p)
        cash = max(cash, hold + p - fee)
    return cash
```

**Follow-up 4 — Unlimited transactions with a cooldown (LeetCode 309)**

Now after I sell, I must sit out one day before I can buy again. The catch is that buying today can no longer draw from yesterday's `cash`, because yesterday's `cash` might have come from a sale, which would violate the cooldown. So I carry one extra value, the `cash` from two days ago, and I buy against that older cash instead. In words, `hold` becomes the better of staying in hold or buying using the cash from two days back, and `cash` updates as before. I just have to remember the previous `cash` before overwriting it each day. Still linear time and constant space.

```python
def maxProfit(prices):
    hold, cash, prev_cash = float('-inf'), 0, 0
    for p in prices:
        hold = max(hold, prev_cash - p)
        prev_cash, cash = cash, max(cash, hold + p)
    return cash
```

**Follow-up 5 — At most k transactions (LeetCode 188)**

The most general version caps the number of transactions at k. I add a dimension for how many transactions I've used and keep a `hold` and a `cash` value for each count from zero up to k. A transaction is counted at the buy, so buying into the j-th transaction draws from the `cash` after finishing j minus one transactions. I sweep every price and, for each transaction count, update `hold` then `cash`. This is the parent of everything above: k equal to one recovers the single-transaction case, and any k at least half the number of days behaves like unlimited. The time is the number of days times k, and the space is proportional to k.

```python
def maxProfit(prices, k):
    hold = [float('-inf')] * (k + 1)
    cash = [0] * (k + 1)
    for p in prices:
        for j in range(1, k + 1):
            hold[j] = max(hold[j], cash[j - 1] - p)
            cash[j] = max(cash[j], hold[j] + p)
    return cash[k]
```

---
