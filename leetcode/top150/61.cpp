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
    ListNode* next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x) : val(x), next(nullptr) {}
    ListNode(int x, ListNode* next) : val(x), next(next) {}
};

class Solution {
  public:
    ListNode* rotateRight(ListNode* head, int k) {
        if (!head) {
            return nullptr;
        }
        int len = 0;
        ListNode* cur = head;
        while (cur->next) {
            ++len;
            cur = cur->next;
        }
        ++len;
        cur->next = head;
        k = len - (k % len);
        cur = head;
        for (int i = 0; i < k - 1; ++i) {
            cur = cur->next;
        }
        head = cur->next;
        cur->next = nullptr;
        return head;
    }
};