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
    void reverse(ListNode* p0, int k) {
        if (!p0->next || !p0->next->next || k == 0) {
            return;
        }
        ListNode* cur = p0->next, *pre = nullptr, *nxt = nullptr;
        for (int i = 0; i < k; ++i) {
            nxt = cur->next;
            cur->next = pre;
            pre = cur;
            cur = nxt;
        }
        p0->next->next = cur;
        p0->next = pre;
    }
  public:
    ListNode* rotateRight(ListNode* head, int k) {
        if (!head) {
            return nullptr;
        }
        ListNode* dummy = new ListNode(0, head);
        int len = 0;
        ListNode* cur = head;
        while (cur) {
            ++len;
            cur = cur->next;
        }
        k = k % len;
        reverse(dummy, len);
        reverse(dummy, k);
        cur = dummy;
        for (int i = 0; i < k; ++i) {
            cur = cur->next;
        }
        reverse(cur, len - k);
        return dummy->next;
    }
};