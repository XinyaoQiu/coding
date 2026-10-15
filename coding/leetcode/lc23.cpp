#include <iostream>
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

    ListNode* mergeTwoLists(ListNode* head1, ListNode* head2) {
        ListNode* p1 = head1;
        ListNode* p2 = head2;
        ListNode* hair = new ListNode();
        ListNode* curr = hair;
        while (p1 && p2) {
            if (p1->val <= p2->val) {
                curr->next = p1;
                curr = p1;
                p1 = p1->next;
            } else {
                curr->next = p2;
                curr = p2;
                p2 = p2->next;
            }
        }
        curr->next = p1 ? p1 : p2;
        return hair->next;
    }

    ListNode* merge(vector<ListNode*>& lists, int L, int R) {
        if (L == R) {
            return lists[L];
        }
        int M = L + (R - L) / 2;
        ListNode* left = merge(lists, L, M);
        ListNode* right = merge(lists, M + 1, R);
        return mergeTwoLists(left, right);
    }

  public:
    ListNode* mergeKLists(vector<ListNode*>& lists) {
        if (lists.empty()) return nullptr;
        return merge(lists, 0, lists.size() - 1);
    }
};