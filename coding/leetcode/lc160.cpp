#include <iostream>
#include <vector>

using namespace std;

class Solution {
    struct ListNode {
        int val;
        ListNode* next;
        ListNode(int x) : val(x), next(NULL) {}
    };

  public:
    ListNode* getIntersectionNode(ListNode* headA, ListNode* headB) {
        ListNode* p1 = headA;
        ListNode* p2 = headB;
        while (1) {
            if (!p1->next && !p2->next) {
                return nullptr;
            }
            if (p1 == p2) {
                return p1;
            }
            p1 = p1->next ? p1->next : headB;
            p2 = p2->next ? p2->next : headA;
        }
    }
};