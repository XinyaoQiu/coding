/*
// Definition for a Node.
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
*/

class Node {
  public:
    int val;
    Node* next;
    Node* random;

    Node(int _val) : val(_val), next(nullptr), random(nullptr) {}
    Node(int _val, Node* _next, Node* _random)
        : val(_val), next(_next), random(_random) {}
};

class Solution {
  public:
    Node* copyRandomList(Node* head) {
        Node* dummy = new Node(0, head, nullptr);
        while (head) {
            head->next = new Node(head->val, head->next, nullptr);
            head = head->next->next;
        }
        head = dummy->next;
        while (head) {
            if (head->random) {
                head->next->random = head->random->next;
            }
            head = head->next->next;
        }
        head = dummy;
        while (head->next) {
            Node* nxt = head->next;
            head->next = nxt->next;
            head = nxt;
        }
        return dummy->next;
    }
};