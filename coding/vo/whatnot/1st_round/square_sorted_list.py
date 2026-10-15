from heapq import *

def func(list1, list2):
    if not list1 and not list2:
        return []
    heap, ans = [], []
    index1, index2 = 0, 0
    for i, n in enumerate(list1):
        if n < 0:
            index1 = i
        else:
            break
    for i, n in enumerate(list2):
        if n < 0:
            index2 = i
        else:
            break
    if 0 <= index1 < len(list1):
        heap.append((list1[index1] ** 2, 0, index1))
    if 0 <= index1 + 1 < len(list1):
        heap.append((list1[index1+1] ** 2, 1, index1 + 1))
    if 0 <= index2 < len(list2):
        heap.append((list2[index2] ** 2, 2, index2))
    if 0 <= index2 + 1 < len(list2):
        heap.append((list2[index2+1] ** 2, 3, index2 + 1))
    heapify(heap)
    print(heap)
    while heap:
        x, flag, i = heappop(heap)
        ans.append(x)
        if flag == 0 and i - 1 >= 0:
            heappush(heap, (list1[i - 1] ** 2, 0, i - 1))
        elif flag == 1 and i + 1 < len(list1):
            heappush(heap, (list1[i + 1] ** 2, 1, i + 1))
        elif flag == 2 and i - 1 >= 0:
            heappush(heap, (list2[i - 1] ** 2, 2, i - 1))
        elif flag == 3 and i + 1 < len(list2):
            heappush(heap, (list2[i + 1] ** 2, 3, i + 1))
    return ans

print(func([-4, -2, 0, 1, 3], [-3, -2, 2, 3]))
