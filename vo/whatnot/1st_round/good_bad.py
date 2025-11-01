def func(status):
    L, R = 0, len(status) - 1
    while L <= R:
        M = L + (R - L) // 2
        if status[M] == 'bad':
            R = M - 1
        else:
            L = M + 1
    return L

print(func(['good', 'good', 'good', 'bad', 'bad', 'bad']))