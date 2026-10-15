"""
validate returns the list of rules that actually failed their own check.

A rule whose dependency did not pass is SKIPPED -- its check never runs, and it
does not appear in the result. This is what makes the DAG load-bearing: the
graph decides which checks execute, and the returned list stays free of rules
that were merely blocked upstream.

Evaluation is memoized DFS. No topological sort needed: the recursion order IS
the topological order, and memo guarantees each node is evaluated once.
"""

PASSED = "PASSED"
FAILED = "FAILED"
SKIPPED = "SKIPPED"


class Order:
    def __init__(self, price, items):
        self.price = price
        self.items = items


OPERATORS = {
    "between": lambda actual, expected: expected[0] <= actual <= expected[1],
    "not_in": lambda actual, expected: not (set(actual) & set(expected)),
    "in": lambda actual, expected: bool(set(actual) & set(expected)),
    "eq": lambda actual, expected: actual == expected,
    "gt": lambda actual, expected: actual > expected,
    "lt": lambda actual, expected: actual < expected,
}

FIELDS = {
    "price": lambda order: order.price,
    "items": lambda order: order.items,
}


class Node:
    def __init__(self, name, field, operator, value):
        self.name = name
        self.field = field
        self.operator = operator
        self.value = value
        self.parents = set()
        self.children = set()

    def __repr__(self):
        return f"Node({self.name})"


class OrderValidator:
    def __init__(self):
        self.nodes = {}

    def add_rule(self, name, field, operator, value, depends_on=None):
        depends_on = depends_on or []
        if name in self.nodes or not name:
            return False
        if not self._valid(field, operator, value):
            return False
        if any(d not in self.nodes for d in depends_on):
            return False
        node = Node(name, field, operator, value)
        self.nodes[name] = node
        for d in depends_on:
            parent = self.nodes[d]
            node.parents.add(parent)
            parent.children.add(node)
        return True

    def remove_rule(self, name):
        if name not in self.nodes:
            return False
        node = self.nodes[name]
        for child in node.children:
            child.parents.discard(node)
            child.parents.update(node.parents)
        for parent in node.parents:
            parent.children.discard(node)
            parent.children.update(node.children)
        del self.nodes[name]
        return True

    def validate(self, order):
        memo = {}
        for node in self.nodes.values():
            self._eval(node, order, memo)
        return [n.name for n, s in memo.items() if s == FAILED]

    def _eval(self, node, order, memo):
        if node in memo:
            return memo[node]
        for parent in node.parents:
            if self._eval(parent, order, memo) != PASSED:
                memo[node] = SKIPPED
                return SKIPPED
        memo[node] = PASSED if self._check(node, order) else FAILED
        return memo[node]

    def _check(self, node, order):
        return OPERATORS[node.operator](FIELDS[node.field](order), node.value)

    def _valid(self, field, operator, value):
        if field not in FIELDS or operator not in OPERATORS:
            return False
        if operator == "between":
            if not isinstance(value, (tuple, list)) or len(value) != 2:
                return False
            lo, hi = value
            if not isinstance(lo, (int, float)) or not isinstance(hi, (int, float)):
                return False
            return lo <= hi
        if operator in ("in", "not_in"):
            return isinstance(value, (set, list, tuple))
        return True
