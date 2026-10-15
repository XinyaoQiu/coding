struct ListNode {
    int val;
    ListNode *next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x) : val(x), next(nullptr) {}
    ListNode(int x, ListNode *next) : val(x), next(next) {}
};

class Solution {
    ListNode* merge(ListNode* head1, ListNode* head2) {
        ListNode* dummy = new ListNode(), *cur = dummy;
        while (head1 && head2) {
            if (head1->val < head2->val) {
                cur->next = head1;
                head1 = head1->next;
            } else {
                cur->next = head2;
                head2 = head2->next;
            }
            cur = cur->next;
        }
        while (head1) {
            cur->next = head1;
            head1 = head1->next;
            cur = cur->next;
        }
        while (head2) {
            cur->next = head2;
            head2 = head2->next;
            cur = cur->next;
        }
        return dummy->next;
    }
public:
    ListNode* sortList(ListNode* head) {
        if (!head || !head->next) {
            return head;
        }
        ListNode* dummy = new ListNode(0, head);
        ListNode* fast = dummy, *slow = dummy;
        while (fast && fast->next) {
            fast = fast->next->next;
            slow = slow->next;
        }
        ListNode* head2 = slow->next;
        slow->next = nullptr;
        ListNode* p1 = sortList(head), *p2 = sortList(head2);
        return merge(p1, p2);
    }
};