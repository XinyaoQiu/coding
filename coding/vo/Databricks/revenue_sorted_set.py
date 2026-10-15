from sortedcontainers import SortedList
from typing import Optional, Set, Dict, List, Tuple

class ReadHeavyReferralSystem:
    def __init__(self):
        self.revenue: Dict[int, int] = {}
        self.referrer: Dict[int, Optional[int]] = {}
        self.children: Dict[int, List[int]] = {}
        self.total: Dict[int, int] = {}
        self.sorted_list: SortedList[Tuple[int, int]] = SortedList()
        self.next_id = 0

    def insert(self, revenue: int, referrer: Optional[int]) -> int:
        id = self.next_id
        self.next_id += 1
        self.revenue[id] = revenue
        self.referrer[id] = referrer
        self.children[id] = []
        self.total[id] = revenue
        self.sorted_list.add((revenue, id))
        if referrer is not None:
            self.children[referrer].append(id)
            old_total = self.total[referrer]
            new_total = old_total + revenue
            self.total[referrer] = new_total
            self.sorted_list.discard((old_total, referrer))
            self.sorted_list.add((new_total, referrer))

        return id

    def get_lowest_k_by_total_revenue(self, k: int, min_total_revenue: int) -> Set[int]:
        idx = self.sorted_list.bisect_left((min_total_revenue, -1))
        result = []
        for total, cid in self.sorted_list.islice(idx, idx + k):
            result.append(cid)
        return set(result)

    def get_relation(self, root_id: int) -> List[Set[int]]:
        from collections import deque
        result = []
        queue = deque([root_id])
        visited = {root_id}
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
