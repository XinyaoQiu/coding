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

### Q2. Best Time to Buy and Sell Stock II (LeetCode 122)

**Problem**

You are given an integer array `prices` where `prices[i]` is the price of a given stock on the i-th day. On each day you may buy and/or sell the stock, but you can hold at most one share at any time. You may complete as many transactions as you like (you can even buy and sell on the same day). Return the maximum profit you can achieve.

Example 1: `prices = [7,1,5,3,6,4]` returns `7`, buying at 1 and selling at 5, then buying at 3 and selling at 6. Example 2: `prices = [1,2,3,4,5]` returns `4`. Example 3: `prices = [7,6,4,3,1]` returns `0`, since prices only fall.

Constraints: array length from one up to about three times ten to the fourth, and prices from zero up to about ten to the fourth.

**Walkthrough**

So this is the same setup as the classic single-transaction stock problem, but the key difference I'd call out first is that now I can make as many transactions as I want, as long as I only hold one share at a time. So instead of finding one best buy-sell pair, I'm after the total profit over many trades.

A general way to think about it is a state machine: each day I'm either holding a share or not, and I track the best cash I can have in each state, carrying both forward day to day. That's the dynamic programming formulation, linear time with constant space, and it's the one that generalizes to all the harder variants.

But there's a much simpler greedy insight specific to this unlimited version. Because I can buy and sell freely, capturing a multi-day rise from a low to a high gives exactly the same total as summing every individual day-over-day increase along the way. So I don't even need to find the peaks and valleys. I just walk the prices, and every time today is higher than yesterday I add that difference to my profit, and I skip every drop. Single pass, linear time, constant space, and it's the cleanest thing to explain.

The main edge case is a strictly decreasing sequence, where no daily difference is positive, so I add nothing and correctly return zero. There's no double counting, because summing consecutive rises is mathematically identical to buying at each valley and selling at each peak.

**Python solution**

Greedy version (cleanest to present):

```python
def maxProfit(prices):
    profit = 0
    for i in range(1, len(prices)):
        if prices[i] > prices[i - 1]:
            profit += prices[i] - prices[i - 1]
    return profit
```

State-machine DP version (the general approach):

```python
def maxProfit(prices):
    hold, cash = float('-inf'), 0
    for p in prices:
        hold = max(hold, cash - p)
        cash = max(cash, hold + p)
    return cash
```

**Follow-up 1 — Handle zero-or-unlimited transactions in one function**

The interviewer may ask to also cover the single-transaction case (buy and sell at most once) with the same code. The state machine is identical; the only difference between the two problems is what money I'm allowed to reuse when I buy: in the unlimited case I can put profit I've already banked back in, so the buy transition draws from my current cash, whereas in the single-transaction case each buy starts fresh from zero. So I gate that one term with a flag. Everything else stays the same linear-time constant-space scan.

```python
def maxProfit(prices, unlimited=True):
    hold, cash = float('-inf'), 0
    for p in prices:
        base = cash if unlimited else 0
        hold = max(hold, base - p)
        cash = max(cash, hold + p)
    return cash
```

**Follow-up 2 — Unlimited transactions with a transaction fee (LeetCode 714)**

Now every completed transaction charges a fixed fee. The greedy trick breaks here, because blindly capturing every small daily rise would lose money to the fee each time, so I fall back to the two-state machine. The only change from the unlimited case is that I subtract the fee once per completed transaction, and I do that at the moment I sell. It stays a single linear pass with constant space, and charging the fee only on sell guarantees I pay it exactly once per round trip.

```python
def maxProfit(prices, fee):
    hold, cash = float('-inf'), 0
    for p in prices:
        hold = max(hold, cash - p)
        cash = max(cash, hold + p - fee)
    return cash
```

**Follow-up 3 — At most k transactions (LeetCode 188)**

The most general version caps the number of transactions at k. I add a dimension for how many transactions I've used, and keep a holding and a not-holding best-cash value for each count from zero to k. A transaction is counted at the buy, so buying into the j-th transaction draws from the cash after finishing j minus one transactions. I sweep every price and, for each transaction count, update the buy and sell values. This is the parent of everything above: k equal to one recovers the single-transaction case, and any k at least half the number of days behaves like the unlimited case. The time is the number of days times k, and the space is proportional to k.

```python
def maxProfit(prices, k):
    buy = [float('-inf')] * (k + 1)
    sell = [0] * (k + 1)
    for p in prices:
        for j in range(1, k + 1):
            buy[j] = max(buy[j], sell[j - 1] - p)
            sell[j] = max(sell[j], buy[j] + p)
    return sell[k]
```

---
