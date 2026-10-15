"""
Variant: edges live on the nodes (rule.parents / rule.children) instead of in
external adjacency dicts on the validator.

Compared to v2:
  - self.depends_on / self.dependents are gone; the validator holds only a name index
  - traversal is rule.parents directly, no name -> object lookup
  - memo is keyed by the Rule object, not by name

Tradeoff: a Rule now belongs to exactly one validator, and the graph is no
longer trivially serializable (object references instead of names).
"""


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
        self.parents = set()
        self.children = set()

    def __repr__(self):
        return f"Rule({self.name})"


class OrderValidator:
    def __init__(self):
        self.rules = {}

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
        for d in rule.depends_on:
            parent = self.rules[d]
            rule.parents.add(parent)
            parent.children.add(rule)
        return True

    def remove_rule(self, name):
        if name not in self.rules:
            return False
        rule = self.rules[name]
        for child in rule.children:
            child.parents.discard(rule)
            child.parents.update(rule.parents)
        for parent in rule.parents:
            parent.children.discard(rule)
            parent.children.update(rule.children)
        rule.parents.clear()
        rule.children.clear()
        del self.rules[name]
        return True

    def validate(self, order):
        memo = {}
        return all(self._eval(rule, order, memo) for rule in self.rules.values())

    def _eval(self, rule, order, memo):
        if rule in memo:
            return memo[rule]
        ok = all(self._eval(p, order, memo) for p in rule.parents) \
            and self._check(rule, order)
        memo[rule] = ok
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
    def snapshot(val):
        return {n: (set(r.parents), set(r.children)) for n, r in val.rules.items()}

    for bad in [
        Rule("no_weapons", "price", "between", (0, 1)),
        Rule("ghost_dep", "price", "between", (0, 1), ["nope"]),
        Rule("self_dep", "price", "between", (0, 1), ["self_dep"]),
        Rule("bad_field", "shipping", "eq", "US"),
        Rule("bad_op", "price", "matches", 5),
        Rule("inverted", "price", "between", (200, 100)),
        Rule("", "price", "between", (0, 100)),
    ]:
        before = snapshot(v)
        assert not v.add_rule(bad)
        assert snapshot(v) == before
        assert bad.parents == set() and bad.children == set()

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

    # remove a middle node: children inherit its parents, both directions rewired
    v = OrderValidator()
    v.add_rule(Rule("p", "price", "between", (0, 10000)))
    v.add_rule(Rule("q", "price", "between", (0, 10000)))
    v.add_rule(Rule("x", "price", "between", (0, 9000), ["p", "q"]))
    v.add_rule(Rule("m", "price", "between", (0, 8000), ["x"]))
    v.add_rule(Rule("n", "items", "not_in", {"gun"}, ["x"]))
    assert v.remove_rule("x")
    names = lambda rules: {r.name for r in rules}
    assert names(v.rules["m"].parents) == {"p", "q"}
    assert names(v.rules["n"].parents) == {"p", "q"}
    assert names(v.rules["p"].children) == {"m", "n"}
    assert names(v.rules["q"].children) == {"m", "n"}
    assert v.validate(Order(100, ["book"]))
    assert not v.validate(Order(50000, ["book"]))

    # remove a root: children lose the parent entirely
    v = OrderValidator()
    v.add_rule(Rule("a", "price", "between", (0, 100)))
    v.add_rule(Rule("b", "price", "between", (0, 10000), ["a"]))
    assert v.remove_rule("a")
    assert v.rules["b"].parents == set()
    assert v.validate(Order(5000, ["book"]))

    # the caller's declared depends_on is never rewritten
    r = Rule("z2", "price", "between", (0, 100), ["z1"])
    v = OrderValidator()
    v.add_rule(Rule("z1", "price", "between", (0, 100)))
    v.add_rule(r)
    v.remove_rule("z1")
    assert r.depends_on == ["z1"]
    assert r.parents == set()

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

    # two same-named Rule objects are distinct nodes -- object identity, not name
    v = OrderValidator()
    r1 = Rule("dup", "price", "between", (0, 100))
    r2 = Rule("dup", "price", "between", (0, 100))
    assert v.add_rule(r1)
    assert not v.add_rule(r2)
    assert v.rules["dup"] is r1

    print("all tests passed")
