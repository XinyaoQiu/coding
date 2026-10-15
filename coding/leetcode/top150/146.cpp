#include <unordered_map>
using namespace std;

class LRUCache {
    struct ListNode {
        int key;
        int val;
        ListNode* next;
        ListNode* prev;
        ListNode() : key(0), val(0), next(nullptr), prev(nullptr) {}
        ListNode(int key, int val)
            : key(key), val(val), next(nullptr), prev(nullptr) {}
    };

    ListNode* dummyHead;
    ListNode* dummyTail;
    unordered_map<int, ListNode*> m;
    int capacity;

  public:
    LRUCache(int capacity) : capacity(capacity) {
        dummyHead = new ListNode();
        dummyTail = new ListNode();
        dummyHead->next = dummyTail;
        dummyTail->prev = dummyHead;
    }

    int get(int key) {
        if (m.count(key)) {
            ListNode* node = m[key];
            move_to_front(node);
            return node->val;
        }
        return -1;
    }

    void put(int key, int value) {
        if (m.count(key)) {
            ListNode* node = m[key];
            node->val = value;
            move_to_front(node);
        } else {
            if (m.size() == capacity) {
                m.erase(dummyTail->prev->key);
                remove(dummyTail->prev);
            }
            ListNode* node = new ListNode(key, value);
            push_front(node);
            m[key] = node;
        }
    }

    void push_front(ListNode* node) {
        node->next = dummyHead->next;
        node->prev = dummyHead;
        dummyHead->next->prev = node;
        dummyHead->next = node;
    }

    void remove(ListNode* node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    void move_to_front(ListNode* node) {
        remove(node);
        push_front(node);
    }
};

/**
 * Your LRUCache object will be instantiated and called as such:
 * LRUCache* obj = new LRUCache(capacity);
 * int param_1 = obj->get(key);
 * obj->put(key,value);
 */