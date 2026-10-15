#include <iostream>
#include <vector>
#include <tuple>

using namespace std;

struct ListNode {
    int value;
    ListNode* next;
    ListNode() : value(0), next(nullptr) {}
    ListNode(int value_) : value(value_), next(nullptr) {}
};

pair<ListNode*, ListNode*> reverse(ListNode* head, ListNode* tail) {
    ListNode* prev = tail->next;
    ListNode* node = head;
    while (prev != tail) {
        ListNode* temp = node;
        node = node->next;
        temp->next = prev;
        prev = temp;
    }
    return { tail, head };
}

ListNode* process(ListNode* head, int k) {
    ListNode* hair = new ListNode();
    hair->next = head;
    ListNode* prev = hair;
    while (head) {
        ListNode* tail = prev;
        for (int i = 0; i < k; i++) {
            tail = tail->next;
            if (!tail) {
                return hair->next;
            }
        }
        tie(head, tail) = reverse(head, tail);
        prev->next = head;
        head = tail->next;
        prev = tail;
    }
    return hair->next;
}

int main() {
    int n, k;
    cin >> n >> k;
    ListNode* hair = new ListNode();
    ListNode* prev = hair;
    for (int i = 0; i < n; i++) {
        int num;
        cin >> num;
        ListNode* node = new ListNode(num);
        prev->next = node;
        prev = node;
    }
    ListNode* head = process(hair->next, k);
    while (head) {
        cout << head->value << " ";
        head = head->next;
    }
    cout << endl;
}