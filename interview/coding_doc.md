# Coding 面试话术模板（情景 → 句式）

> 对话英文，标注中文。核心：**不出现无授权的沉默**，**不说自己解释不了的词**。
> 用法：背句式，不背解法。空槽 `___` 现场填。

---

## 0. 时间分配（45 分钟）

| 阶段 | 时长 | 关键动作 |
|---|---|---|
| 开场宣告 | 10 秒 | 给沉默提前授权 |
| 读题 | 2–4 分钟 | 允许沉默 |
| 复述 + 澄清 | 2–3 分钟 | 你复述，他确认 |
| 思路 + 复杂度 | 3–5 分钟 | 先讲再写 |
| 假设 + edge case | 1 分钟 | **必须在写码之前** |
| 写码 | 10–15 分钟 | 边写边说 |
| 走测试 | 3–5 分钟 | 手动跑 example |
| 优化讨论 | 5 分钟 | 主动提 |

---

## 1. 开场

**情景：刚拿到题，要开始读**
> "Let me take a couple of minutes to read through this. Once I'm done, I'll restate the problem, then walk you through my approach before I start coding."

**情景：读到一半，需要更多时间**
> "This one's a bit longer than I expected — give me another minute."

**情景：英文题面读得慢，想提前说明**
> "English problem statements take me a little longer to parse — I might need three or four minutes here. Is that okay?"

**情景：不想有任何沉默，边读边说**
> "Okay, so the input is ___... there's a constraint on ___... let me look at the first example..."

---

## 2. 读题与理解

**情景：复述题目（永远第一步）**
> "Let me restate the problem to make sure we're aligned. I'm given ___, and I need to return ___. Is that right?"

**情景：走一个 example（性价比最高的 30 秒）**
> "Let me walk through example one. Given ___, the expected output is ___. My understanding of why: ___."

**情景：题面太长，想先压缩**
> "Let me summarize what I think the core task is, and you can correct me: ___."

**情景：读不懂某一段（说出来，不要硬啃）**
> "I want to make sure I'm reading this part correctly — when it says ___, does that mean ___?"

**情景：把约束翻译成解法决策**
> "Looking at the constraints — n goes up to ___, so an O(n²) approach would be too slow. That points me toward ___."

> "Values can be negative, so I can't assume ___."

**情景：定点提问（问具体点，不问整题）**
> "Quick clarification — does ___ here mean ___, or ___?"

> "Can the input be empty? And is it guaranteed to contain only ___?"

**情景：二维数组 / 矩阵，行与行是否连续**
> "Are the rows contiguous — does the last element of one row connect to the first element of the next?"

**情景：题目术语有歧义（如 vacation / valid / longest）**
> "Just to pin down the definition — by ___, do you mean ___, or ___?"

---

## 3. 开放题（只给一句话，什么都没有）

**情景：题目没有输入输出定义**
> "The problem is open-ended, so let me first pin down the interface. I'll assume the input is ___ and I should return ___. Does that match what you have in mind?"

**情景：没有样例，自己造一个**
> "Let me make up a small example so we're working from the same picture. If the input is ___, I'd expect ___ — because ___. Does that look right?"

**情景：没有约束，自己问出来**
> "A few things aren't specified: roughly how large can the input get? Can there be duplicates? Are negative values possible? Is the input already sorted?"

> "Should I optimize for time or for memory here?"

**情景：明确说出假设（说了就不算你的错）**
> "I'm going to assume ___ and ___. If either is wrong, let me know and I'll adjust."

---

## 4. 讲思路

**情景：说方案（先讲再写）**
> "Here's my approach. I'll ___. Concretely: first ___, then ___, and finally ___."

**情景：只想到暴力解（完全合法的开场）**
> "Let me start with the brute force and then optimize. The naive way is ___, which is O(___). Let me see if I can do better."

**情景：有多个方案，说取舍（强信号）**
> "There are two ways I could go. The brute force is ___, which is O(n²). The better option is ___ using ___, which gets it to O(n) at the cost of O(n) extra space. I'll go with the second unless memory is a concern."

**情景：报复杂度**
> "Time complexity is O(___), space is O(___) for the ___."

**情景：主动论证下界（在他问之前先说掉）**
> "I don't think we can do better than O(___) here — we have to examine every ___ to know the answer, so linear is the lower bound. Space is O(1), which is also minimal."

**情景：声明遍历时维护什么（别只说 "iterate"）**
> "I'll do a single pass, tracking ___ and ___, along with ___."

**情景：写码前说 edge case（unprompted，评分差一档）**
> "Before I start coding, let me note the edge cases I want to handle: ___, ___, and the case where no valid answer exists — I'll return ___ for that. Does that work for you?"

**情景：请求开始写**
> "If that approach sounds good, I'll go ahead and code it up."

---

## 5. 写码中

**情景：每个逻辑块开头（不用念每行）**
> "I'll start with the input validation..."

> "Now the main loop — I'm iterating through ___ and tracking ___."

> "Here I'm updating ___."

**情景：需要安静想一会儿（报备过的沉默不扣分）**
> "Give me twenty seconds to think through the ___."

**情景：自己发现 bug（主动认，是正分）**
> "Hold on — I just realized this breaks when ___. Let me fix that: ___."

**情景：忘了 API 细节（问，不要硬编）**
> "I don't remember the exact signature of ___ offhand — I'll write it as ___ and we can assume it behaves like ___."

---

## 6. 写完之后（不等他问）

**情景：手动走样例**
> "Let me trace through example one. Starting with i = 0, ___ is 0... at i = ___ we hit ___ so ___ becomes ___... final answer is ___, which matches."

**情景：主动过 edge case**
> "Now let me check the edge cases I mentioned. Empty input — the loop doesn't execute and we return ___, correct. ___ — we return ___, also correct."

**情景：主动谈优化取舍**
> "One thing worth noting: I'm using O(n) extra space. If memory were tight, I could ___ at the cost of ___. Happy to go that route if you'd prefer."

---

## 7. 应急句式（最该背熟的部分）

**情景：被问"有没有更优解"——你已经最优**

先想下界：需不需要读完全部输入？需要 → O(n) 是硬下界。**永远不要慌乱中报一个更小的复杂度。**

> "I believe O(___) is already optimal — we have to examine every element to know the answer, so we can't do better than linear. Do you see room I'm missing?"

**情景：被问"有没有更优解"——你不确定**
> "Let me think about that for a second rather than guess."

**情景：确实可能更优，想找结构**
> "If the input had additional structure — say it were sorted, or given as a list of ___ instead of ___ — then I could do better. With this representation I think linear is the floor."

**情景：被纠正了（接得好比一开始答对分还高）**
> "Got it — so ___ comes from ___, not from ___. That changes my model: I'd ___, and then it becomes ___. Let me redo my approach with that."

**情景：卡住了（不要沉默）**
> "Let me think out loud. I know I need ___, but I'm not sure how to handle ___ efficiently. My instinct is ___ — does that seem like a reasonable direction?"

**情景：想要提示**
> "Could you give me a hint about which part I should be looking at?"

**情景：被问 "hello?" / 怀疑掉线**
> "Sorry — still here, just reading. I'll have a summary for you in about a minute."

**情景：时间快到没写完**
> "I'm aware of the time. Let me tell you how I'd finish this: ___. The remaining part is ___."

**情景：反问环节**
> "What does the team's day-to-day look like — how much is new feature development versus maintaining existing systems?"

> "What would a successful first six months look like in this role?"

---

## 8. 速查（截图用）

| 情景 | 句式 |
|---|---|
| **开场** | "Let me take a couple of minutes to read through this. Then I'll restate the problem and walk you through my approach before coding." |
| **复述** | "Let me restate the problem. I'm given ___, and I need to return ___. Is that right?" |
| **走样例** | "Let me walk through example one. Given ___, the output is ___. My understanding of why: ___." |
| **边界/约束** | "Can I assume ___?" / "If the input is empty, what should I return — 0 or -1?" |
| **讲思路** | "Here's my approach. I'll ___. Concretely: first ___, then ___, finally ___." |
| **报复杂度** | "Time O(___), space O(___). I don't think we can do better — we have to examine every ___ to know the answer." |
| **开始写** | "Before coding, the edge cases I see: ___, ___. More may surface as I write — I'll call them out. If that sounds good, I'll code it up." |
| **中途发现 case** | "Writing this out surfaced a case I hadn't considered: ___. Let me handle it." |

**卡住时**（这三句最该背熟）

| 情景 | 句式 |
|---|---|
| 卡住 | "Let me think out loud. I know I need ___, but I'm not sure how to handle ___. My instinct is ___ — reasonable direction?" |
| 被问"能更优吗" | "I believe O(___) is already optimal — we have to examine every element, so linear is the floor. Do you see room I'm missing?" |
| 被纠正 | "Got it — so ___ comes from ___, not ___. That changes my model: I'd ___. Let me redo my approach." |

**Edge case 从代码读**：`for i:=0` → 空/单元素 ｜ `for i:=1` → n=1 ｜ `arr[i±1]` → 越界 ｜ `if` 无 `else` → 条件恒假 ｜ 返回 -1 的分支 → 让它走到
**四类输入**：空/最小 ｜ 全同 `[1,1,1]` ｜ 答案不存在 ｜ 答案在首/尾

**铁律**

1. 超过 15 秒的沉默，前面必须有一句授权。
2. 解释不了的术语一个字都别说。
3. 被问"能更优吗"先想下界，敢站住；不确定说"让我想一下"，**绝不猜复杂度**。
4. 中途发现 edge case 不扣分，归因于代码结构，别说"我忘了"。
5. 想 20 秒 → 说出来 → 再想 20 秒。交替，不并行。

### 七条铁律

1. 超过 15 秒的沉默，前面必须有一句授权。
2. 解释不了的术语一个字都别说——他追问的永远是你说过的话。
3. edge case 在写码**之前**说，并留开口。
4. 被问"能不能更优"先想下界，敢站住；不确定就说"让我想一下"，**绝不猜一个复杂度**。
5. 复述题目那 30 秒是免费思考时间——嘴说套话，脑子想解法。
6. 写码中发现新 edge case 不扣分，**说法要归因于代码结构**，不说"我忘了"。
7. 交替，不是并行——想 20 秒 → 说出来 → 再想 20 秒。

---

## 9. 练习法（针对边想边说困难）

**问题不是英语水平，是带宽**：想解法 + 翻译英文同时占工作记忆，必然崩。解法是把说话变成不过脑子的调用。

1. **抄句式**：上面的句式挑自己说得顺的版本，拗口的换简单的。面试不考词汇量，"I'm stuck" 好过 "I seem to have reached an impasse"。
2. **录音自述**：拿**已经会做**的题（Two Sum 级别），全英文从复述讲到复杂度，录音。用会做的题是为了 100% 带宽给表达。同一题讲三遍，第三遍会明显顺。
3. **补漏**：听回放，卡壳的地方就是缺的句式，补进第 1 步的清单。
4. **加负载**：顺了之后换没做过的题。这时再卡，卡的是思路不是英语。
5. **读题专项**：LeetCode hard 只读题不解题，90 秒内用两句话概括输入输出。

**交替，不是并行**：不需要真的边想边说。可以"想 20 秒 → 说出来 → 再想 20 秒"，只要沉默前有授权句。
