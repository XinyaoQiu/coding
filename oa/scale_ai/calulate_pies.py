from collections import Counter

note = "dough butter apple"
recipe = [
    {"ingredient": "dough", "count": 1, "subrecipe": {"butter": 1, "sugar": 2}},
    {"ingredient": "butter", "count": 2}
]

def dollars_to_cents(s):
    return int(float(s.replace("$","")) * 100)

def expand(ingredient, count, recipe_map, memo):
    key = (ingredient, count)
    if key in memo:
        return memo[key]
    sub = recipe_map.get(ingredient)
    if sub is None:
        memo[key] = Counter({ingredient: count})
        return memo[key]
    result = Counter()
    for sub_ing, sub_cnt in sub.items():
        result += expand(sub_ing, sub_cnt * count, recipe_map, memo)
    memo[key] = result
    return result  


def calculate_pie_profit(note, recipe, ingredient_prices, bids):
    recipe_map = {}
    for item in recipe:
        recipe_map[item['ingredient']] = item.get('subrecipe')
    print(f"recipe_map = {recipe_map}")
    memo, pie_need = {}, Counter()
    for item in recipe:
        pie_need += expand(item['ingredient'], item['count'], recipe_map, memo)
    print(f"pie_need = {pie_need}")
    note_counter = Counter(note.split())
    note_base = Counter()
    for ing, cnt in note_counter.items():
        note_base += expand(ing, cnt, recipe_map, memo)
    print(f"note_base = {note_base}")
    max_pies_from_inventory = min(
        note_base[ing] // need
        for ing, need in pie_need.items()
    )
    ingredient_prices_cents = {k: dollars_to_cents(v) for k, v in ingredient_prices.items()}
    pie_cost = 0
    for ing, need in pie_need.items():
        if ing not in ingredient_prices_cents:
            ingredient_prices_cents[ing] = 0  # treat unspecified ingredient price as 0
        pie_cost += ingredient_prices_cents[ing] * need

    # 6) Collect all bids, filter profitable ones
    offers = []
    for buyer, info in bids.items():
        count = info["count"]
        price_cents = dollars_to_cents(info["price"])
        if price_cents >= pie_cost:
            offers.append((price_cents, count))  # (unit_price, count)

    # No profitable buyers
    if not offers:
        return 0

    # 7) Sell pies in descending order of price to maximize profit
    offers.sort(reverse=True)  # highest price first

    pies_left = max_pies_from_inventory
    profit = 0

    for price_cents, want_cnt in offers:
        if pies_left == 0:
            break
        sell_cnt = min(pies_left, want_cnt)
        profit += sell_cnt * (price_cents - pie_cost)
        pies_left -= sell_cnt

    return profit

calculate_pie_profit(note, recipe, {}, {})