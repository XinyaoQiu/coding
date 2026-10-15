from collections import Counter
from heapq import *



def dollars_to_cents(s):
    return int(float(s.replace("$","")) * 100)

def expand(ingredient, count, recipe_map, counter):
    sub = recipe_map.get(ingredient)
    if sub is None:
        counter[ingredient] += count
        return
    for sub_ing, sub_cnt in sub.items():
        expand(sub_ing, sub_cnt * count, recipe_map, counter)


def calculate_pie_profit(note, recipe, ingredient_prices, bids):
    recipe_map = {}
    for item in recipe:
        recipe_map[item['ingredient']] = item.get('subrecipe')
    print(f"recipe_map = {recipe_map}")
    pie_need = Counter()
    for item in recipe:
        expand(item['ingredient'], item['count'], recipe_map, pie_need)
    print(f"pie_need = {pie_need}")
    note_counter = Counter(note.split())
    note_base = Counter()
    for ing, cnt in note_counter.items():
        expand(ing, cnt, recipe_map, note_base)
    print(f"note_base = {note_base}")
    max_pies_from_inventory = min(
        note_base[ing] // need
        for ing, need in pie_need.items()
    )
    ingredient_prices_cents = {k: dollars_to_cents(v) for k, v in ingredient_prices.items()}
    pie_cost = 0
    for ing, need in pie_need.items():
        pie_cost += ingredient_prices_cents[ing] * need
    offers = []
    for _, info in bids.items():
        count = info["count"]
        price_cents = dollars_to_cents(info["price"])
        if price_cents >= pie_cost:
            offers.extend([price_cents] * count)
    if not offers:
        return 0
    k = min(len(offers), max_pies_from_inventory)
    heap = offers[:k]
    for i in range(k, len(offers)):
        heappushpop(heap, offers[i])
    return sum(heap) - len(heap) * pie_cost

    
note = "dough butter dough butter dough butter apple"
recipe = [
    {"ingredient": "dough", "count": 1, "subrecipe": {"butter": 1, "sugar": 1}},
    {"ingredient": "butter", "count": 1}
]
ingredient_prices = {'butter': '$1.00', 'sugar': '$2.00'}
bids = {'Alice': {'count': 1, 'price': '$5.00'}, 'Bob': {'count': 3, 'price': '$6.00'}}
print(calculate_pie_profit(note, recipe, ingredient_prices, bids))