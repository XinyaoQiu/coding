"""
Transcipt:
Interviewer: Here is the problem.
I: OK I can see it. I'll ask AI tool to restate the problem and I'll think through it.
I: Can I assume frequency for validate calling is much larger than the frequency for rule adding or removing?
I: Can I assume if rule A fails and rule B depends on rule A, rule B will fail too?
I: What if there are conflicts when I try to add a rule or delete a rule? Just reject and return False?
I: I will cache the result for validate function. After I add or remove a rule, I will invalidate the cache, 
and the next validate will re-calculate the result.
I: Now the complexity for validate is O(V), and O(V + E) for add_rule, and O(V + E) for remove_rule. Totally 
space complexity is O(V + E) for the whole validator.
I: Then I'll tell AI agent to write the OrderValidator. And some test cases.
"""


"""
Implement a OrderValidator, determine whether there're illegal items in orders, or whether the order's price 
is inside some range. You can add or delete rules. There is a DAG relationship for all rules.
"""

from collections import deque, defaultdict

class Order:
    def __init__(self, price, items):
        self.price = price
        self.items = items


class Rule:
    def __init__(self, name, type, values, depends_on=None):
        self.name = name
        self.type = type
        self.values = values
        self.depends_on = depends_on or []

class OrderValidator:
    def __init__(self):
        self.rules = {}
        self.depends_on = defaultdict(set)
        self.dependents = defaultdict(set)
        self.order_cache = None

    def validate_rule(self, rule):
        if not rule.name:
            return False
        if rule.type == "illegal_items":
            return isinstance(rule.values, (set, list, tuple))
        elif rule.type == "price_range":
            if not isinstance(rule.values, (tuple, list)) or len(rule.values) != 2:
                return False
            lo, hi = rule.values
            if not isinstance(lo, (int, float)) or not isinstance(hi, (int, float)):
                return False
            return lo <= hi
        else:
            return False

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
        self.order_cache = None
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
        self.order_cache = None
        return True

    def validate(self, order):
        passed = set()
        for name in self.topo_order():
            if not self.depends_on[name] <= passed:
                return False
            if not self.check(self.rules[name], order):
                return False
            passed.add(name)
        return True

    def check(self, rule, order):
        if rule.type == "illegal_items":
            return not (set(rule.values) & set(order.items))
        elif rule.type == "price_range":
            lo, hi = rule.values
            return lo <= order.price <= hi
        else:
            return False

    def topo_order(self):
        if self.order_cache is not None:
            return self.order_cache
        indeg = {n: len(self.depends_on[n]) for n in self.rules}
        q = deque(n for n, d in indeg.items() if d == 0)
        out = []
        while q:
            n = q.popleft()
            out.append(n)
            for child in self.dependents[n]:
                indeg[child] -= 1
                if indeg[child] == 0:
                    q.append(child)
        self.order_cache = out
        return out


if __name__ == "__main__":
    # basic: two rules, b depends on a
    v = OrderValidator()
    assert v.add_rule(Rule("no_weapons", "illegal_items", {"gun", "knife"}))
    assert v.add_rule(Rule("under_1000", "price_range", (0, 1000), ["no_weapons"]))
    assert v.validate(Order(500, ["book", "pen"]))
    assert not v.validate(Order(5000, ["book"]))
    assert not v.validate(Order(500, ["gun"]))
    assert not v.validate(Order(5000, ["gun"]))

    # validate_rule: shape checks isolated from add_rule's other rejection paths
    assert not v.validate_rule(Rule("x", "price_range", (200, 100)))     # inverted
    assert not v.validate_rule(Rule("x", "price_range", (100,)))         # too short
    assert not v.validate_rule(Rule("x", "price_range", (1, 2, 3)))      # too long
    assert not v.validate_rule(Rule("x", "price_range", {"gun", "knife"}))
    assert not v.validate_rule(Rule("x", "price_range", ("a", "b")))     # non-numeric
    assert not v.validate_rule(Rule("x", "price_range", None))
    assert not v.validate_rule(Rule("x", "illegal_items", "gun"))        # bare string
    assert not v.validate_rule(Rule("x", "illegal_items", None))
    assert not v.validate_rule(Rule("x", "shipping_zone", ["US"]))       # unknown type
    assert not v.validate_rule(Rule("", "price_range", (0, 100)))        # empty name
    assert v.validate_rule(Rule("x", "price_range", (100, 100)))
    assert v.validate_rule(Rule("x", "price_range", [0, 10000]))
    assert v.validate_rule(Rule("x", "illegal_items", set()))

    # a rejected add leaves no partial state behind
    for bad in [
        Rule("no_weapons", "price_range", (0, 1)),          # duplicate name
        Rule("ghost_dep", "price_range", (0, 1), ["nope"]),  # dangling dependency
        Rule("self_dep", "price_range", (0, 1), ["self_dep"]),
        Rule("bad_type", "shipping_zone", ["US"]),
        Rule("r_inverted", "price_range", (200, 100)),
        Rule("", "price_range", (0, 100)),
    ]:
        before = (set(v.rules), {k: set(s) for k, s in v.depends_on.items()},
                  {k: set(s) for k, s in v.dependents.items()})
        assert not v.add_rule(bad)
        assert set(v.rules) == before[0]
        assert {k: set(s) for k, s in v.depends_on.items()} == before[1]
        assert {k: set(s) for k, s in v.dependents.items()} == before[2]

    # valid boundary shapes are still accepted
    assert v.add_rule(Rule("r_equal_bounds", "price_range", (100, 100)))
    assert v.add_rule(Rule("r_list_range", "price_range", [0, 10000]))
    assert v.add_rule(Rule("r_empty_items", "illegal_items", set()))

    # remove_rule rejection
    assert not v.remove_rule("nope")

    # empty validator accepts everything
    assert OrderValidator().validate(Order(999999, ["gun"]))

    # independent rules, no edges
    v = OrderValidator()
    v.add_rule(Rule("r1", "illegal_items", {"gun"}))
    v.add_rule(Rule("r2", "price_range", (0, 1000)))
    assert v.validate(Order(500, ["book"]))
    assert not v.validate(Order(500, ["gun"]))
    assert not v.validate(Order(5000, ["book"]))

    # chain of three, failure propagates from the root
    v = OrderValidator()
    v.add_rule(Rule("c", "price_range", (0, 10000)))
    v.add_rule(Rule("b", "illegal_items", {"drug"}, ["c"]))
    v.add_rule(Rule("a", "price_range", (100, 500), ["b"]))
    assert v.validate(Order(300, ["book"]))
    assert not v.validate(Order(50000, ["book"]))

    # diamond: b,c depend on a; d depends on b,c
    v = OrderValidator()
    v.add_rule(Rule("a", "price_range", (0, 10000)))
    v.add_rule(Rule("b", "price_range", (0, 9000), ["a"]))
    v.add_rule(Rule("c", "illegal_items", {"gun"}, ["a"]))
    v.add_rule(Rule("d", "price_range", (0, 8000), ["b", "c"]))
    assert len(v.topo_order()) == 4
    assert v.validate(Order(100, ["book"]))
    assert not v.validate(Order(50000, ["book"]))

    # remove a middle node: dependents inherit its dependencies
    v = OrderValidator()
    v.add_rule(Rule("p", "price_range", (0, 10000)))
    v.add_rule(Rule("q", "price_range", (0, 10000)))
    v.add_rule(Rule("x", "price_range", (0, 9000), ["p", "q"]))
    v.add_rule(Rule("m", "price_range", (0, 8000), ["x"]))
    v.add_rule(Rule("n", "illegal_items", {"gun"}, ["x"]))
    assert v.remove_rule("x")
    assert v.depends_on["m"] == {"p", "q"}
    assert v.depends_on["n"] == {"p", "q"}
    assert v.dependents["p"] == {"m", "n"}
    assert v.dependents["q"] == {"m", "n"}
    assert len(v.topo_order()) == 4
    assert v.validate(Order(100, ["book"]))
    assert not v.validate(Order(50000, ["book"]))

    # remove a root: dependents lose the dependency entirely
    v = OrderValidator()
    v.add_rule(Rule("a", "price_range", (0, 100)))
    v.add_rule(Rule("b", "price_range", (0, 10000), ["a"]))
    assert v.remove_rule("a")
    assert v.depends_on["b"] == set()
    assert v.validate(Order(5000, ["book"]))

    # cache invalidation: adding a rule changes the verdict
    v = OrderValidator()
    v.add_rule(Rule("r1", "price_range", (0, 10000)))
    assert v.validate(Order(5000, ["book"]))
    v.add_rule(Rule("r2", "price_range", (0, 1000)))
    assert not v.validate(Order(5000, ["book"]))
    assert v.remove_rule("r2")
    assert v.validate(Order(5000, ["book"]))

    # the validator does not mutate the caller's Rule
    r = Rule("z2", "price_range", (0, 100), ["z1"])
    v = OrderValidator()
    v.add_rule(Rule("z1", "price_range", (0, 100)))
    v.add_rule(r)
    v.remove_rule("z1")
    assert r.depends_on == ["z1"]

    # price_range boundaries are inclusive
    v = OrderValidator()
    v.add_rule(Rule("r", "price_range", (100, 200)))
    assert v.validate(Order(100, []))
    assert v.validate(Order(200, []))
    assert not v.validate(Order(99, []))
    assert not v.validate(Order(201, []))

    # illegal_items: empty order, duplicates, case sensitivity
    v = OrderValidator()
    v.add_rule(Rule("r", "illegal_items", {"gun"}))
    assert v.validate(Order(10, []))
    assert not v.validate(Order(10, ["gun", "gun"]))
    assert v.validate(Order(10, ["Gun"]))

    print("all tests passed")


