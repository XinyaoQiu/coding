#include "leetcode_base.h"

struct ListNode {
    int val;
    ListNode* next;
    ListNode(int x) : val(x), next(NULL) {}
};

class Solution {
  public:
    bool hasCycle(ListNode* head) {
        if (!head || !head->next) {
            return false;
        }
        ListNode* fast = head->next->next;
        ListNode* slow = head->next;
        while (fast != slow) {
            if (!fast || !fast->next) {
                return false;
            }
            fast = fast->next->next;
            slow = slow->next;
        }
        return true;
    }
};