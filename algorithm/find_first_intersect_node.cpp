#include <iostream>

using namespace std;

struct ListNode {
    int value;
    ListNode* next;
    ListNode() : value(0), next(nullptr) {}
    ListNode(int value_) : value(value_), next(nullptr) {}
};

class Solution {
    ListNode* findFirstCircle(ListNode* head) {
        ListNode* slow = head;
        ListNode* fast = head;
        while (fast && fast->next) {
            slow = slow->next;
            fast = fast->next->next;
            if (slow == fast) {
                fast = head;
                while (slow != fast) {
                    slow = slow->next;
                    fast = fast->next;
                }
                return slow;
            }
        }   
        return nullptr;
    }

    ListNode* findIfNoCircle(ListNode* head1, ListNode* head2, ListNode* tail) {
        ListNode* p1 = head1;
        ListNode* p2 = head2;
        while (p1 != p2) {
            p1 = p1 != tail ? p1->next : head2;
            p2 = p2 != tail ? p2->next : head1;
        }
        return p1;
    }

  public:
    ListNode* findFirstIntersectNode(ListNode* head1, ListNode* head2) {
        ListNode* loop1 = findFirstCircle(head1);
        ListNode* loop2 = findFirstCircle(head2);

        if (!loop1 && !loop2) {
            return findIfNoCircle(head1, head2, nullptr);
        } else if (loop1 && loop2) {
            if (loop1 == loop2) {
                return findIfNoCircle(head1, head2, loop1);
            } else {
                ListNode* curr = loop1->next;
                while (curr != loop1) {
                    if (curr == loop2) {
                        return loop1;
                    }
                    curr = curr->next;
                }
                return nullptr;
            }
        }
    }
}