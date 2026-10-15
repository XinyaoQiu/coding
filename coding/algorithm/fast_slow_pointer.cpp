#include <iostream>
#include <tuple>
#include <vector>

using namespace std;

struct ListNode {
    int value;
    ListNode *next;
    ListNode() : value(0), next(nullptr) {}
    ListNode(int value_) : value(value_), next(nullptr) {}
};

pair<ListNode*, ListNode*> process(ListNode* head) {
    ListNode* fast = head->next;
    ListNode* slow = head;
    while (fast && fast->next) {
        fast = fast->next->next;
        slow = slow->next;
    }
    return {fast, slow};
}

int main() {
    ListNode *hair = new ListNode();
    ListNode *node = hair;
    int n, num;
    cin >> n;
    for (int i = 0; i < n; i++) {
        cin >> num;
        ListNode *temp = new ListNode(num);
        node->next = temp;
        node = temp;
    }
    ListNode* fast;
    ListNode* slow;
    tie(fast, slow) = process(hair->next);
    cout << slow->value << endl;
}