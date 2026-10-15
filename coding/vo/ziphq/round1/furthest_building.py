from typing import *
from heapq import *

def furthestBuilding(heights: List[int], bricks: int, ladders: int) -> int:
    heap = []
    bricks_sum = 0
    i = 0
    while i < len(heights) - 1:
        diff = heights[i+1] - heights[i]
        if diff > 0:
            heappush(heap, diff)
            if len(heap) > ladders:
                bricks_sum += heappop(heap)
                if bricks_sum > bricks:
                    break
        i += 1
    return i

heights = [4,2,7,6,9,14,12]
bricks = 5
ladders = 1
print(furthestBuilding(heights, bricks, ladders))
heights = [4,12,2,7,3,18,20,3,19]
bricks = 10
ladders = 2
print(furthestBuilding(heights, bricks, ladders))

# def func2(heights: List[int], bricks: int, ladders: int) -> int:
#     heap = []
#     bricks_sum = 0
#     i = 0
#     no_ops = set()
#     while i < len(heights) - 1:
#         diff = heights[i+1] - heights[i]
#         if diff > 0:
#             if len(heap) < ladders:
#                 heappush(heap, (diff, i))
#             else:
#                 op = heappushpop(heap, (diff, i))
#                 bricks_sum += op[0]
#                 if bricks_sum > bricks:
#                     heappush(heap, op)
#                     break
#         else:
#             no_ops.add(i)
#         i += 1
#     print(heap)
#     ladder_ops = set([x for _, x in heap])
#     path = []
#     for j in range(i):
#         if j in no_ops:
#             path.append('no_op')
#         elif j in ladder_ops:
#             path.append('ladder')
#         else:
#             path.append('brick')
#     print(path)
#     return i

# heights = [4,2,7,6,9,14,12]
# bricks = 5
# ladders = 1
# print(func2(heights, bricks, ladders))
# heights = [4,12,2,7,3,18,20,3,19]
# bricks = 10
# ladders = 2
# print(func2(heights, bricks, ladders))