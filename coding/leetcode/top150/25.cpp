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
    ListNode* reverseKGroup(ListNode* head, int k) {
        ListNode* dummy = new ListNode(0, head);
        ListNode* p0 = dummy;
        int len = 0;
        while (head) {
            head = head->next;
            ++len;
        }
        head = dummy->next;
        for (int i = 0; i < len / k; ++i) {
            ListNode *pre, *nxt;
            for (int j = 0; j < k; ++j) {
                nxt = head->next;
                head->next = pre;
                pre = head;
                head = nxt;
            }
            nxt = p0->next;
            nxt->next = head;
            p0->next = pre;
            p0 = nxt;
        }
        return dummy->next;
    }
};