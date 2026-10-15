"""
Rewrite of OrderValidator using the pattern common to production rule engines
(jeyabalajis/simple-rule-engine, venmo/business-rules):

  - field + operator + value are three separate things, not one "type" string
  - evaluation is memoized DFS, not an explicit topological sort
  - dependencies are pulled on demand; the recursion order IS the topological order

The graph stays acyclic structurally: a new rule may only depend on existing
rules and has no dependents at insertion time, so it cannot close a cycle.
Reconnection on removal only ever replaces a path with a shorter one, so it
cannot create one either. That is why plain memoized DFS suffices -- no
three-color marking needed.
"""

from collections import defaultdict


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


class Rule:
    def __init__(self, name, field, operator, value, depends_on=None):
        self.name = name
        self.field = field
        self.operator = operator
        self.value = value
        self.depends_on = depends_on or []


class OrderValidator:
    def __init__(self):
        self.rules = {}
        self.depends_on = defaultdict(set)
        self.dependents = defaultdict(set)

    def validate_rule(self, rule):
        if not rule.name:
            return False
        if rule.field not in FIELDS or rule.operator not in OPERATORS:
            return False
        if rule.operator == "between":
            if not isinstance(rule.value, (tuple, list)) or len(rule.value) != 2:
                return False
            lo, hi = rule.value
            if not isinstance(lo, (int, float)) or not isinstance(hi, (int, float)):
                return False
            return lo <= hi
        if rule.operator in ("in", "not_in"):
            return isinstance(rule.value, (set, list, tuple))
        return True

    def add_rule(self, rule):
        if rule.name in self.rules:
            return False
        if not self.validate_rule(rule):
            return False
        if any(d not in self.rules for d in rule.depends_on):
            return False
        self.rules[rule.name] = rule
        self.depends_on[rule.name] = set(rule.depends_on)
        for d in rule.depends_on:
            self.dependents[d].add(rule.name)
        return True

    def remove_rule(self, name):
        if name not in self.rules:
            return False
        deps = self.depends_on[name]
        children = self.dependents[name]
        for child in children:
            self.depends_on[child].discard(name)
            self.depends_on[child].update(deps)
        for d in deps:
            self.dependents[d].discard(name)
            self.dependents[d].update(children)
        del self.rules[name]
        self.depends_on.pop(name, None)
        self.dependents.pop(name, None)
        return True

    def validate(self, order):
        memo = {}
        return all(self._eval(name, order, memo) for name in self.rules)

    def _eval(self, name, order, memo):
        if name in memo:
            return memo[name]
        ok = all(self._eval(d, order, memo) for d in self.depends_on[name]) \
            and self._check(self.rules[name], order)
        memo[name] = ok
        return ok

    def _check(self, rule, order):
        actual = FIELDS[rule.field](order)
        return OPERATORS[rule.operator](actual, rule.value)


if __name__ == "__main__":
    # basic: two rules, b depends on a
    v = OrderValidator()
    assert v.add_rule(Rule("no_weapons", "items", "not_in", {"gun", "knife"}))
    assert v.add_rule(Rule("under_1000", "price", "between", (0, 1000), ["no_weapons"]))
    assert v.validate(Order(500, ["book", "pen"]))
    assert not v.validate(Order(5000, ["book"]))
    assert not v.validate(Order(500, ["gun"]))
    assert not v.validate(Order(5000, ["gun"]))

    # validate_rule: shape checks isolated from add_rule's other rejection paths
    assert not v.validate_rule(Rule("x", "price", "between", (200, 100)))
    assert not v.validate_rule(Rule("x", "price", "between", (100,)))
    assert not v.validate_rule(Rule("x", "price", "between", (1, 2, 3)))
    assert not v.validate_rule(Rule("x", "price", "between", {"gun", "knife"}))
    assert not v.validate_rule(Rule("x", "price", "between", ("a", "b")))
    assert not v.validate_rule(Rule("x", "price", "between", None))
    assert not v.validate_rule(Rule("x", "items", "not_in", "gun"))
    assert not v.validate_rule(Rule("x", "items", "not_in", None))
    assert not v.validate_rule(Rule("x", "shipping", "eq", "US"))
    assert not v.validate_rule(Rule("x", "price", "matches", 5))
    assert not v.validate_rule(Rule("", "price", "between", (0, 100)))
    assert v.validate_rule(Rule("x", "price", "between", (100, 100)))
    assert v.validate_rule(Rule("x", "price", "between", [0, 10000]))
    assert v.validate_rule(Rule("x", "items", "not_in", set()))

    # a rejected add leaves no partial state behind
    for bad in [
        Rule("no_weapons", "price", "between", (0, 1)),
        Rule("ghost_dep", "price", "between", (0, 1), ["nope"]),
        Rule("self_dep", "price", "between", (0, 1), ["self_dep"]),
        Rule("bad_field", "shipping", "eq", "US"),
        Rule("bad_op", "price", "matches", 5),
        Rule("inverted", "price", "between", (200, 100)),
        Rule("", "price", "between", (0, 100)),
    ]:
        before = (set(v.rules), {k: set(s) for k, s in v.depends_on.items()},
                  {k: set(s) for k, s in v.dependents.items()})
        assert not v.add_rule(bad)
        assert set(v.rules) == before[0]
        assert {k: set(s) for k, s in v.depends_on.items()} == before[1]
        assert {k: set(s) for k, s in v.dependents.items()} == before[2]

    assert not v.remove_rule("nope")
    assert OrderValidator().validate(Order(999999, ["gun"]))

    # independent rules, no edges
    v = OrderValidator()
    v.add_rule(Rule("r1", "items", "not_in", {"gun"}))
    v.add_rule(Rule("r2", "price", "between", (0, 1000)))
    assert v.validate(Order(500, ["book"]))
    assert not v.validate(Order(500, ["gun"]))
    assert not v.validate(Order(5000, ["book"]))

    # chain of three, failure propagates from the root
    v = OrderValidator()
    v.add_rule(Rule("c", "price", "between", (0, 10000)))
    v.add_rule(Rule("b", "items", "not_in", {"drug"}, ["c"]))
    v.add_rule(Rule("a", "price", "between", (100, 500), ["b"]))
    assert v.validate(Order(300, ["book"]))
    assert not v.validate(Order(50000, ["book"]))

    # diamond: b,c depend on a; d depends on b,c -- shared subexpression
    v = OrderValidator()
    v.add_rule(Rule("a", "price", "between", (0, 10000)))
    v.add_rule(Rule("b", "price", "between", (0, 9000), ["a"]))
    v.add_rule(Rule("c", "items", "not_in", {"gun"}, ["a"]))
    v.add_rule(Rule("d", "price", "between", (0, 8000), ["b", "c"]))
    assert v.validate(Order(100, ["book"]))
    assert not v.validate(Order(50000, ["book"]))

    # memoization: the shared node is evaluated exactly once
    calls = []
    orig = OrderValidator._check
    OrderValidator._check = lambda self, rule, order: (calls.append(rule.name), orig(self, rule, order))[1]
    v.validate(Order(100, ["book"]))
    OrderValidator._check = orig
    assert calls.count("a") == 1, calls
    assert sorted(calls) == ["a", "b", "c", "d"], calls

    # remove a middle node: dependents inherit its dependencies
    v = OrderValidator()
    v.add_rule(Rule("p", "price", "between", (0, 10000)))
    v.add_rule(Rule("q", "price", "between", (0, 10000)))
    v.add_rule(Rule("x", "price", "between", (0, 9000), ["p", "q"]))
    v.add_rule(Rule("m", "price", "between", (0, 8000), ["x"]))
    v.add_rule(Rule("n", "items", "not_in", {"gun"}, ["x"]))
    assert v.remove_rule("x")
    assert v.depends_on["m"] == {"p", "q"}
    assert v.depends_on["n"] == {"p", "q"}
    assert v.dependents["p"] == {"m", "n"}
    assert v.dependents["q"] == {"m", "n"}
    assert v.validate(Order(100, ["book"]))
    assert not v.validate(Order(50000, ["book"]))

    # remove a root: dependents lose the dependency entirely
    v = OrderValidator()
    v.add_rule(Rule("a", "price", "between", (0, 100)))
    v.add_rule(Rule("b", "price", "between", (0, 10000), ["a"]))
    assert v.remove_rule("a")
    assert v.depends_on["b"] == set()
    assert v.validate(Order(5000, ["book"]))

    # the validator does not mutate the caller's Rule
    r = Rule("z2", "price", "between", (0, 100), ["z1"])
    v = OrderValidator()
    v.add_rule(Rule("z1", "price", "between", (0, 100)))
    v.add_rule(r)
    v.remove_rule("z1")
    assert r.depends_on == ["z1"]

    # between is inclusive on both ends
    v = OrderValidator()
    v.add_rule(Rule("r", "price", "between", (100, 200)))
    assert v.validate(Order(100, []))
    assert v.validate(Order(200, []))
    assert not v.validate(Order(99, []))
    assert not v.validate(Order(201, []))

    # not_in: empty order, duplicates, case sensitivity
    v = OrderValidator()
    v.add_rule(Rule("r", "items", "not_in", {"gun"}))
    assert v.validate(Order(10, []))
    assert not v.validate(Order(10, ["gun", "gun"]))
    assert v.validate(Order(10, ["Gun"]))

    # adding an operator needs no engine change
    OPERATORS["gte"] = lambda actual, expected: actual >= expected
    v = OrderValidator()
    assert v.add_rule(Rule("min_price", "price", "gte", 50))
    assert v.validate(Order(100, []))
    assert not v.validate(Order(10, []))

    print("all tests passed")
