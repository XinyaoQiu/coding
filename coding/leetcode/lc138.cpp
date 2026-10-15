#include <iostream>
#include <vector>

using namespace std;

class Node {
  public:
    int val;
    Node* next;
    Node* random;

    Node(int _val) {
        val = _val;
        next = NULL;
        random = NULL;
    }
};

class Solution {
  public:
    Node* copyRandomList(Node* head) {
        if (!head) {
            return nullptr;
        }
        Node* curr = head;
        while (curr) {
            Node* temp = curr->next;
            Node* node = new Node(curr->val);
            curr->next = node;
            node->next = temp;
            curr = temp;
        }
        curr = head;
        Node* newHead = head->next;
        Node* newCurr = newHead;
        while (curr) {
            newCurr->random = curr->random ? curr->random->next : nullptr;
            curr = curr->next->next;
            newCurr = newCurr->next ? newCurr->next->next : nullptr;
        }
        curr = head;
        newCurr = newHead;
        while (curr) {
            Node* temp = newCurr->next;
            curr->next = temp;
            newCurr->next = temp ? temp->next : nullptr;
            curr = temp;
            newCurr = temp ? temp->next : nullptr;
        }
        return newHead;
    }
};