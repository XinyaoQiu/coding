from typing import Optional, Set, List, Dict
from heapq import *
from collections import deque

class ReferralSystem:
    def __init__(self):
        self.revenue: Dict[int, int] = {}
        self.referrer: Dict[int, Optional[int]] = {}
        self.children: Dict[int, List[int]] = {}
        self.total: Dict[int, int] = {}   
        self.next_id = 0

    def insert(self, revenue: int, referrer: Optional[int]) -> int:
        id = self.next_id
        self.next_id += 1
        self.revenue[id] = revenue
        self.referrer[id] = referrer
        self.children[id] = []
        self.total[id] = revenue
        if referrer is not None:
            self.children[referrer].append(id)
            self.total[referrer] += revenue
        return id

    def get_lowest_k_by_total_revenue(self, k: int, min_total_revenue: int) -> Set[int]:
        filtered = [
            (self.total[id], id)
            for id in self.total
            if self.total[id] >= min_total_revenue
        ]
        topk = nsmallest(k, filtered)
        return {i for _, i in topk}

    def getRelation(self, root_id: int) -> List[Set[int]]:
        result = []
        queue = deque([root_id])
        visited = set([root_id])

        while queue:
            level_size = len(queue)
            level_nodes = set()
            for _ in range(level_size):
                cur = queue.popleft()
                for nxt in self.children.get(cur, []):
                    if nxt not in visited:
                        visited.add(nxt)
                        queue.append(nxt)
                        level_nodes.add(nxt)
            if level_nodes:
                result.append(level_nodes)
        return result

refsys = ReferralSystem()
refsys.insert(5, None)
refsys.insert(3, 0)
refsys.insert(6, None)
refsys.insert(4, 1)
refsys.insert(2, None)
print(refsys.get_lowest_k_by_total_revenue(3, 3))