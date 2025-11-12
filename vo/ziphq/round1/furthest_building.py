from typing import *
from heapq import *

def furthestBuilding(heights: List[int], bricks: int, ladders: int) -> int:
    heap = []
    bricks_sum = 0
    i = 0
    while i < len(heights) - 1:
        diff = heights[i+1] - heights[i]
        if diff > 0:
            if len(heap) < ladders:
                heappush(heap, diff)
            else:
                bricks_sum += heappushpop(heap, diff)
                if bricks_sum > bricks:
                    break
        i += 1
    return i

heights = [4,2,7,6,9,14,12]
bricks = 5
ladders = 1
print(furthestBuilding(heights, bricks, ladders))