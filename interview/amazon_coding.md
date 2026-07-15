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

<!-- 待用户提供题目后逐题填入。每题模板：

### Q<n>. <Problem Title> (LeetCode <id>)

**Problem**

<statement>

**Walkthrough**

<spoken-style English, no formatting / formulas / code>

**Python solution**

```python
<code>
```

---
-->
