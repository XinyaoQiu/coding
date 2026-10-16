# AI Interview Questions

## 1. Why are you interested in working on AI agents?

I’m interested in how agents turn natural language into useful actions. My background is in backend systems, so I’m especially interested in making those actions reliable—handling tool failures, managing state, and preventing unintended changes.

## 2. What is an AI agent, and how is it different from a chatbot?

A chatbot mainly responds to messages. An agent can use tools, take actions, and adjust its next steps based on the results. That makes it useful for multi-step tasks, but it also means mistakes can affect real systems.

## 3. How do you use AI tools in your development workflow?

I use AI to explain unfamiliar code, explore implementation options, and suggest edge cases. I review the output and verify it with tests and documentation. I’m especially careful with changes involving permissions, data deletion, or sensitive information.

> 准备提示：按实际使用方式调整，最好准备一个具体例子。

## 4. What do you think are the biggest challenges in building AI agents?

I think reliability is a major challenge. A model might misunderstand the request, choose the wrong tool, or repeat an action after a timeout. I’d use clear tool interfaces, permission checks, bounded retries, and monitoring to make failures easier to detect and contain.

## 5. How would you balance an agent’s autonomy with safety?

I’d base the level of autonomy on the impact of the action. Reading information could happen automatically, while deleting data or sending sensitive information would need stronger checks and explicit approval. I’d enforce permissions in backend code rather than relying only on the model to follow instructions.
