"""
Interview-shaped version: Node is an internal structure owned by the validator,
the way ListNode belongs to LRUCache. Edges live on the nodes.

add_rule(name, field, operator, value, depends_on) constructs the Node itself,
so there is no caller-owned object to worry about and no stale depends_on field.
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
        return all(self._eval(node, order, memo) for node in self.nodes.values())

    def _eval(self, node, order, memo):
        if node in memo:
            return memo[node]
        ok = all(self._eval(p, order, memo) for p in node.parents) \
            and self._check(node, order)
        memo[node] = ok
        return ok

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


if __name__ == "__main__":
    names = lambda nodes: {n.name for n in nodes}

    # basic: two rules, under_1000 depends on no_weapons
    v = OrderValidator()
    assert v.add_rule("no_weapons", "items", "not_in", {"gun", "knife"})
    assert v.add_rule("under_1000", "price", "between", (0, 1000), ["no_weapons"])
    assert v.validate(Order(500, ["book", "pen"]))
    assert not v.validate(Order(5000, ["book"]))
    assert not v.validate(Order(500, ["gun"]))
    assert not v.validate(Order(5000, ["gun"]))

    # _valid: shape checks isolated from add_rule's other rejection paths
    assert not v._valid("price", "between", (200, 100))
    assert not v._valid("price", "between", (100,))
    assert not v._valid("price", "between", (1, 2, 3))
    assert not v._valid("price", "between", {"gun", "knife"})
    assert not v._valid("price", "between", ("a", "b"))
    assert not v._valid("price", "between", None)
    assert not v._valid("items", "not_in", "gun")
    assert not v._valid("items", "not_in", None)
    assert not v._valid("shipping", "eq", "US")
    assert not v._valid("price", "matches", 5)
    assert v._valid("price", "between", (100, 100))
    assert v._valid("price", "between", [0, 10000])
    assert v._valid("items", "not_in", set())

    # a rejected add leaves no partial state behind
    snapshot = lambda val: {n: (names(x.parents), names(x.children))
                            for n, x in val.nodes.items()}
    for args in [
        ("no_weapons", "price", "between", (0, 1), None),      # duplicate name
        ("ghost", "price", "between", (0, 1), ["nope"]),        # dangling dep
        ("self_dep", "price", "between", (0, 1), ["self_dep"]),
        ("bad_field", "shipping", "eq", "US", None),
        ("bad_op", "price", "matches", 5, None),
        ("inverted", "price", "between", (200, 100), None),
        ("", "price", "between", (0, 100), None),               # empty name
    ]:
        before = snapshot(v)
        assert not v.add_rule(*args)
        assert snapshot(v) == before
        assert args[0] not in v.nodes or args[0] == "no_weapons"

    assert not v.remove_rule("nope")
    assert OrderValidator().validate(Order(999999, ["gun"]))

    # independent rules, no edges
    v = OrderValidator()
    v.add_rule("r1", "items", "not_in", {"gun"})
    v.add_rule("r2", "price", "between", (0, 1000))
    assert v.validate(Order(500, ["book"]))
    assert not v.validate(Order(500, ["gun"]))
    assert not v.validate(Order(5000, ["book"]))

    # chain of three, failure propagates from the root
    v = OrderValidator()
    v.add_rule("c", "price", "between", (0, 10000))
    v.add_rule("b", "items", "not_in", {"drug"}, ["c"])
    v.add_rule("a", "price", "between", (100, 500), ["b"])
    assert v.validate(Order(300, ["book"]))
    assert not v.validate(Order(50000, ["book"]))

    # diamond: b,c depend on a; d depends on b,c
    v = OrderValidator()
    v.add_rule("a", "price", "between", (0, 10000))
    v.add_rule("b", "price", "between", (0, 9000), ["a"])
    v.add_rule("c", "items", "not_in", {"gun"}, ["a"])
    v.add_rule("d", "price", "between", (0, 8000), ["b", "c"])
    assert v.validate(Order(100, ["book"]))
    assert not v.validate(Order(50000, ["book"]))

    # memoization: the shared node is evaluated exactly once
    calls = []
    orig = OrderValidator._check
    OrderValidator._check = lambda self, node, order: (calls.append(node.name), orig(self, node, order))[1]
    v.validate(Order(100, ["book"]))
    OrderValidator._check = orig
    assert calls.count("a") == 1, calls
    assert sorted(calls) == ["a", "b", "c", "d"], calls

    # remove a middle node: children inherit its parents, both directions rewired
    v = OrderValidator()
    v.add_rule("p", "price", "between", (0, 10000))
    v.add_rule("q", "price", "between", (0, 10000))
    v.add_rule("x", "price", "between", (0, 9000), ["p", "q"])
    v.add_rule("m", "price", "between", (0, 8000), ["x"])
    v.add_rule("n", "items", "not_in", {"gun"}, ["x"])
    assert v.remove_rule("x")
    assert names(v.nodes["m"].parents) == {"p", "q"}
    assert names(v.nodes["n"].parents) == {"p", "q"}
    assert names(v.nodes["p"].children) == {"m", "n"}
    assert names(v.nodes["q"].children) == {"m", "n"}
    assert v.validate(Order(100, ["book"]))
    assert not v.validate(Order(50000, ["book"]))

    # remove a root: children lose the parent entirely
    v = OrderValidator()
    v.add_rule("a", "price", "between", (0, 100))
    v.add_rule("b", "price", "between", (0, 10000), ["a"])
    assert v.remove_rule("a")
    assert v.nodes["b"].parents == set()
    assert v.validate(Order(5000, ["book"]))

    # between is inclusive on both ends
    v = OrderValidator()
    v.add_rule("r", "price", "between", (100, 200))
    assert v.validate(Order(100, []))
    assert v.validate(Order(200, []))
    assert not v.validate(Order(99, []))
    assert not v.validate(Order(201, []))

    # not_in: empty order, duplicates, case sensitivity
    v = OrderValidator()
    v.add_rule("r", "items", "not_in", {"gun"})
    assert v.validate(Order(10, []))
    assert not v.validate(Order(10, ["gun", "gun"]))
    assert v.validate(Order(10, ["Gun"]))

    # adding an operator needs no engine change
    OPERATORS["gte"] = lambda actual, expected: actual >= expected
    v = OrderValidator()
    assert v.add_rule("min_price", "price", "gte", 50)
    assert v.validate(Order(100, []))
    assert not v.validate(Order(10, []))

    print("all tests passed")
