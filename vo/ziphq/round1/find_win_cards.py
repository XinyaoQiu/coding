from collections import Counter

def pong(cnt, i, flag):
    cnt[i] += 3 * flag

def chow(cnt, i, flag):
    cnt[i] += flag
    cnt[i+1] += flag
    cnt[i+2] += flag

def eyes(cnt, i, flag):
    cnt[i] += 2 * flag

def can_form_melds(cnt):
    if sum(cnt.values()) == 0:
        return True
    i = min(k for k, v in cnt.items() if v > 0)
    if cnt[i] >= 3:
        pong(cnt, i, -1)
        if can_form_melds(cnt):
            pong(cnt, i, 1)
            return True
        pong(cnt, i, 1)
    if i <= 7 and cnt[i+1] > 0 and cnt[i+2] > 0:
        chow(cnt, i, -1)
        if can_form_melds(cnt):
            chow(cnt, i, 1)
            return True
        chow(cnt, i, 1)
    return False

def is_hu(cnt):
    for i in range(1, 10):
        if cnt[i] >= 2:
            eyes(cnt, i, -1)
            if can_form_melds(cnt):
                eyes(cnt, i, 1)
                return True
            eyes(cnt, i, 1)
    return False

def find_win_cards(hand):
    base = Counter(hand)
    res = []
    for x in range(1, 10):
        if base[x] < 4:
            base[x] += 1
            if is_hu(base):
                res.append(x)
            base[x] -= 1
    return res

print(find_win_cards([1, 1, 1, 4, 4, 4, 5, 6, 7, 7, 7, 7, 8]))
