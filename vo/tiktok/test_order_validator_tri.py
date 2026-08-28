from order_validator_tri import OPERATORS, Order, OrderValidator


def names(nodes):
    return {n.name for n in nodes}


def make_chain():
    """no_weapons <- under_1000"""
    v = OrderValidator()
    v.add_rule("no_weapons", "items", "not_in", {"gun", "knife"})
    v.add_rule("under_1000", "price", "between", (0, 1000), ["no_weapons"])
    return v


def make_diamond():
    """b, c depend on a; d depends on b and c"""
    v = OrderValidator()
    v.add_rule("a", "price", "between", (0, 10000))
    v.add_rule("b", "price", "between", (0, 9000), ["a"])
    v.add_rule("c", "items", "not_in", {"gun"}, ["a"])
    v.add_rule("d", "price", "between", (0, 8000), ["b", "c"])
    return v


def test_all_pass():
    v = make_chain()
    assert v.validate(Order(500, ["book"])) == []


def test_leaf_rule_fails_alone():
    """under_1000 fails on its own; its dependency passed"""
    v = make_chain()
    assert v.validate(Order(5000, ["book"])) == ["under_1000"]


def test_dependency_failure_skips_dependent():
    """no_weapons fails, so under_1000 is skipped and not reported"""
    v = make_chain()
    assert v.validate(Order(500, ["gun"])) == ["no_weapons"]


def test_skipped_rule_would_have_failed_too():
    """price is also out of range, but under_1000 is skipped, not reported"""
    v = make_chain()
    assert v.validate(Order(5000, ["gun"])) == ["no_weapons"]


def test_skipped_check_is_never_executed():
    v = make_chain()
    calls = []
    original = OrderValidator._check

    def counting_check(self, node, order):
        calls.append(node.name)
        return original(self, node, order)

    OrderValidator._check = counting_check
    v.validate(Order(5000, ["gun"]))
    OrderValidator._check = original

    assert calls == ["no_weapons"]


def test_empty_validator_reports_nothing():
    assert OrderValidator().validate(Order(999999, ["gun"])) == []


def test_independent_rules_both_reported():
    """no edges, so both failures show up -- neither can skip the other"""
    v = OrderValidator()
    v.add_rule("r1", "items", "not_in", {"gun"})
    v.add_rule("r2", "price", "between", (0, 1000))
    assert sorted(v.validate(Order(5000, ["gun"]))) == ["r1", "r2"]


def test_failure_propagates_down_a_chain():
    """c fails at the root; b and a are skipped, so only c is reported"""
    v = OrderValidator()
    v.add_rule("c", "price", "between", (0, 10000))
    v.add_rule("b", "items", "not_in", {"drug"}, ["c"])
    v.add_rule("a", "price", "between", (100, 500), ["b"])
    assert v.validate(Order(300, ["book"])) == []
    assert v.validate(Order(50000, ["book"])) == ["c"]


def test_diamond_root_failure_reports_only_the_root():
    v = make_diamond()
    assert v.validate(Order(50000, ["book"])) == ["a"]


def test_diamond_partial_failure():
    """c fails on items; d is skipped, but b still runs and passes"""
    v = make_diamond()
    assert v.validate(Order(100, ["gun"])) == ["c"]


def test_diamond_all_pass():
    v = make_diamond()
    assert v.validate(Order(100, ["book"])) == []


def test_shared_node_evaluated_once():
    v = make_diamond()
    calls = []
    original = OrderValidator._check

    def counting_check(self, node, order):
        calls.append(node.name)
        return original(self, node, order)

    OrderValidator._check = counting_check
    v.validate(Order(100, ["book"]))
    OrderValidator._check = original

    assert calls.count("a") == 1
    assert sorted(calls) == ["a", "b", "c", "d"]


def test_between_is_inclusive():
    v = OrderValidator()
    v.add_rule("r", "price", "between", (100, 200))
    assert v.validate(Order(99, [])) == ["r"]
    assert v.validate(Order(100, [])) == []
    assert v.validate(Order(200, [])) == []
    assert v.validate(Order(201, [])) == ["r"]


def test_not_in():
    v = OrderValidator()
    v.add_rule("r", "items", "not_in", {"gun"})
    assert v.validate(Order(10, [])) == []
    assert v.validate(Order(10, ["book"])) == []
    assert v.validate(Order(10, ["gun"])) == ["r"]
    assert v.validate(Order(10, ["gun", "gun"])) == ["r"]
    assert v.validate(Order(10, ["Gun"])) == []


def test_new_operator_needs_no_engine_change():
    OPERATORS["gte"] = lambda actual, expected: actual >= expected
    v = OrderValidator()
    assert v.add_rule("min_price", "price", "gte", 50) is True
    assert v.validate(Order(100, [])) == []
    assert v.validate(Order(10, [])) == ["min_price"]
    del OPERATORS["gte"]


def test_valid_rejects_malformed():
    v = OrderValidator()
    assert v._valid("price", "between", (200, 100)) is False
    assert v._valid("price", "between", (100,)) is False
    assert v._valid("price", "between", (1, 2, 3)) is False
    assert v._valid("price", "between", {"gun", "knife"}) is False
    assert v._valid("price", "between", ("a", "b")) is False
    assert v._valid("price", "between", None) is False
    assert v._valid("items", "not_in", "gun") is False
    assert v._valid("items", "not_in", None) is False
    assert v._valid("shipping", "eq", "US") is False
    assert v._valid("price", "matches", 5) is False


def test_valid_accepts_good_shapes():
    v = OrderValidator()
    assert v._valid("price", "between", (100, 100)) is True
    assert v._valid("price", "between", [0, 10000]) is True
    assert v._valid("items", "not_in", set()) is True
    assert v._valid("items", "not_in", []) is True


def test_add_rule_rejections():
    v = make_chain()
    assert v.add_rule("no_weapons", "price", "between", (0, 1)) is False
    assert v.add_rule("x", "price", "between", (0, 1), ["nope"]) is False
    assert v.add_rule("x", "price", "between", (0, 1), ["x"]) is False
    assert v.add_rule("", "price", "between", (0, 100)) is False
    assert v.add_rule("bad_field", "shipping", "eq", "US") is False
    assert v.add_rule("bad_op", "price", "matches", 5) is False
    assert v.add_rule("inverted", "price", "between", (200, 100)) is False


def test_rejection_leaves_no_partial_state():
    v = make_chain()
    before = {n: (names(x.parents), names(x.children)) for n, x in v.nodes.items()}

    v.add_rule("no_weapons", "price", "between", (0, 1))
    v.add_rule("ghost", "price", "between", (0, 1), ["nope"])
    v.add_rule("self_dep", "price", "between", (0, 1), ["self_dep"])
    v.add_rule("bad_field", "shipping", "eq", "US")
    v.add_rule("inverted", "price", "between", (200, 100))
    v.add_rule("", "price", "between", (0, 100))

    after = {n: (names(x.parents), names(x.children)) for n, x in v.nodes.items()}
    assert after == before


def test_edges_wired_in_both_directions():
    v = OrderValidator()
    v.add_rule("a", "price", "between", (0, 100))
    v.add_rule("b", "price", "between", (0, 100), ["a"])
    assert names(v.nodes["b"].parents) == {"a"}
    assert names(v.nodes["a"].children) == {"b"}


def test_remove_unknown_rule():
    assert OrderValidator().remove_rule("nope") is False


def test_remove_middle_node_reconnects():
    v = OrderValidator()
    v.add_rule("p", "price", "between", (0, 10000))
    v.add_rule("q", "price", "between", (0, 10000))
    v.add_rule("x", "price", "between", (0, 9000), ["p", "q"])
    v.add_rule("m", "price", "between", (0, 8000), ["x"])
    v.add_rule("n", "items", "not_in", {"gun"}, ["x"])

    assert v.remove_rule("x") is True

    assert names(v.nodes["m"].parents) == {"p", "q"}
    assert names(v.nodes["n"].parents) == {"p", "q"}
    assert names(v.nodes["p"].children) == {"m", "n"}
    assert names(v.nodes["q"].children) == {"m", "n"}
    assert v.validate(Order(100, ["book"])) == []
    assert sorted(v.validate(Order(50000, ["book"]))) == ["p", "q"]


def test_remove_root_orphans_children():
    v = OrderValidator()
    v.add_rule("a", "price", "between", (0, 100))
    v.add_rule("b", "price", "between", (0, 10000), ["a"])
    assert v.remove_rule("a") is True
    assert v.nodes["b"].parents == set()
    assert v.validate(Order(5000, ["book"])) == []


def test_remove_leaf():
    v = make_chain()
    assert v.remove_rule("under_1000") is True
    assert names(v.nodes["no_weapons"].children) == set()
    assert v.validate(Order(999999, ["book"])) == []


def test_remove_twice():
    v = make_chain()
    assert v.remove_rule("under_1000") is True
    assert v.remove_rule("under_1000") is False


def test_remove_diamond_root():
    v = make_diamond()
    assert v.remove_rule("a") is True
    assert v.nodes["b"].parents == set()
    assert v.nodes["c"].parents == set()
    assert names(v.nodes["d"].parents) == {"b", "c"}


def test_deep_chain():
    v = OrderValidator()
    for i in range(50):
        deps = [f"r{i - 1}"] if i else None
        assert v.add_rule(f"r{i}", "price", "between", (0, 10000), deps) is True
    assert v.validate(Order(500, [])) == []
    assert v.validate(Order(50000, [])) == ["r0"]
