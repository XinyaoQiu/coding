#include <iostream>
#include <tuple>
#include <vector>

using namespace std;

struct ListNode {
    int val;
    ListNode* next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x) : val(x), next(nullptr) {}
    ListNode(int x, ListNode* next) : val(x), next(next) {}
};

class Solution {
  public:
    pair<ListNode*, ListNode*> reverseList(ListNode* head, ListNode* tail) {
        ListNode* prev = tail->next;
        ListNode* curr = head;
        while (curr != tail->next) {
            auto temp = curr;
            curr = curr->next;
            temp->next = prev;
            prev = temp;
        }
        return {tail, head};
    }
    ListNode* reverseKGroup(ListNode* head, int k) {
        ListNode* hair = new ListNode(0);
        hair->next = head;
        ListNode* prev = hair;
        while (head) {
            ListNode* tail = head;
            for (int i = 0; i < k; i++) {
                tail = tail->next;
                if (!tail) {
                    return hair->next;
                }
            }
            tie(head, tail) = reverseList(head, tail);
            prev->next = head;
            prev = tail;
            head = tail->next;
        }
        return hair->next;
    }
};