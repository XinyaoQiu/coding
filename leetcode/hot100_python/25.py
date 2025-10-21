from typing import *

class ListNode:
    def __init__(self, x=0, next=None):
        self.val = x
        self.next = next
class Solution:
    def reverseKGroup(self, head: Optional[ListNode], k: int) -> Optional[ListNode]:
        length = 0
        curr = head
        while curr:
            length += 1
            curr = curr.next
        dummy = ListNode(0, head)
        p0, curr = dummy, head
        while length >= k:
            length -= k
            pre, nxt = None, None
            for _ in range(k):
                nxt = curr.next
                curr.next = pre
                pre = curr
                curr = nxt
            nxt = p0.next
            nxt.next = curr
            p0.next = pre
            p0 = nxt
        return dummy.next
        