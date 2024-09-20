#include <iostream>
#include <vector>

using namespace std;

struct ListNode {
    int value;
    ListNode* next;
    ListNode() : value(0), next(nullptr) {}
    ListNode(int value_) : value(value_), next(nullptr) {}
};

ListNode* isCircle(ListNode* head) {
    ListNode* fast = head;
    ListNode* slow = head;
    while (fast && fast->next) {
        fast = fast->next->next;
        slow = slow->next;
        if (fast == slow) {
            fast = head;
            while (fast != slow) {
                fast = fast->next;
                slow = slow->next;
            }
            return fast;
        }
    }
    return nullptr;
}

int main() {
    ListNode* head = new ListNode(0);
    ListNode* p1 = new ListNode(1);
    ListNode* p2 = new ListNode(2);
    ListNode* p3 = new ListNode(3);
    ListNode* p4 = new ListNode(4);
    ListNode* p5 = new ListNode(5);
    ListNode* p6 = new ListNode(6);
    ListNode* p7 = new ListNode(7);
    ListNode* p8 = new ListNode(8);
    head->next = p1;
    p1->next = p2;
    p2->next = p3;
    p3->next = p4;
    p4->next = p5;
    p5->next = p6;
    p6->next = p7;
    p7->next = p8;
    p8->next = p4;
    ListNode* res = isCircle(head);
    cout << (res ? res->value : -1) << endl;
}