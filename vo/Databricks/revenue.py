from typing import Optional, Set, List, Dict
from heapq import *

class ReferralSystem:
    def __init__(self):
        self.revenue: List[int] = []
        self.referrer: List[Optional[int]] = []
        self.children: Dict[int, List[int]] = {}
        self.total: List[int] = []

    def insert(self, revenue: int, referrer: Optional[int]) -> int:
        id = len(self.revenue)
        self.revenue.append(revenue)
        self.referrer.append(referrer)
        self.children[id] = []
        self.total.append(revenue)

        if referrer is not None:
            self.children[referrer].append(id)
            self.total[referrer] += revenue

        return id

    def get_lowest_k_by_total_revenue(self, k: int, min_total_revenue: int) -> Set[int]:
        candidates = [
            (self.total[i], i)
            for i in range(len(self.total))
            if self.total[i] >= min_total_revenue
        ]
        topk = nsmallest(k, candidates)
        return {i for _, i in topk}

refsys = ReferralSystem()
refsys.insert(5, None)
refsys.insert(3, 0)
refsys.insert(6, None)
refsys.insert(4, 1)
refsys.insert(2, None)
print(refsys.get_lowest_k_by_total_revenue(3, 3))