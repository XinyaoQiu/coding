#include <iostream>
#include <tuple>
#include <sstream>

using namespace std;

struct ListNode {
    int value;
    ListNode* next;
    ListNode() : value(0), next(nullptr) {}
    ListNode(int value_) : value(value_), next(nullptr) {}
};

ListNode* findMid(ListNode* head) {
    ListNode* fast = head->next;
    ListNode* slow = head;
    while (fast && fast->next) {
        fast = fast->next->next;
        slow = slow->next;
    }
    return slow;
}

ListNode* reverse(ListNode* head) {
    ListNode* prev = nullptr;
    while (head) {
        ListNode* temp = head;
        head = head->next;
        temp->next = prev;
        prev = temp;
    }
    return prev;
}

bool check(ListNode* p1, ListNode* p2) {
    while (p1 && p2) {
        if (p1->value != p2->value) {
            return false;
        }
        p1 = p1->next;
        p2 = p2->next;
    }
    return true;
}

bool process(ListNode* head) {
    if (!head->next) {
        return true;
    }
    ListNode* mid = findMid(head);
    mid->next = reverse(mid->next);
    ListNode* p1 = head;
    ListNode* p2 = mid->next;
    bool result = check(p1, p2);
    mid->next = reverse(mid->next);
    return result;
}

int main() {
    string line;
    getline(cin, line);
    istringstream iss(line);
    int num;
    ListNode* hair = new ListNode();
    ListNode* node = hair;
    while (iss >> num) {
        ListNode* temp = new ListNode(num);
        node->next = temp;
        node = temp;
    }
    cout << process(hair->next) << endl;
}