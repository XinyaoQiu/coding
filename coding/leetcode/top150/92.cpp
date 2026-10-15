/**
 * Definition for singly-linked list.
 * struct ListNode {
 *     int val;
 *     ListNode *next;
 *     ListNode() : val(0), next(nullptr) {}
 *     ListNode(int x) : val(x), next(nullptr) {}
 *     ListNode(int x, ListNode *next) : val(x), next(next) {}
 * };
 */

struct ListNode {
    int val;
    ListNode *next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x) : val(x), next(nullptr) {}
    ListNode(int x, ListNode *next) : val(x), next(next) {}
};

class Solution {
public:
    ListNode* reverseBetween(ListNode* head, int left, int right) {
        ListNode* dummy = new ListNode(0, head);
        ListNode* p0 = dummy;
        for (int i = 0; i < left - 1; ++i) {
            p0 = p0->next;
        }
        head = p0->next;
        ListNode *pre, *nxt;
        for (int i = left; i <= right; ++i) {
            nxt = head->next;
            head->next = pre;
            pre = head;
            head = nxt;
        }
        p0->next->next = head;
        p0->next = pre;
        return dummy->next;
    }
};