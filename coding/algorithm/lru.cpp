#include <unordered_map>

using namespace std;

struct Node {
    int key, value;
    Node *prev;
    Node *next;
    Node() : key(0), value(0), prev(nullptr), next(nullptr) {}
    Node(int key_, int value_)
        : key(key_), value(value_), prev(nullptr), next(nullptr) {}
};

class LRUCache {

  private:
    int capacity;
    int size;
    Node *head;
    Node *tail;
    unordered_map<int, Node *> map;

  public:
    LRUCache(int capacity_) : capacity(capacity_), size(0) {
        head = new Node();
        tail = new Node();
        head->next = tail;
        tail->prev = head;
    }

    int get(int key) {
        if (!map.count(key)) {
            return -1;
        }
        Node* node = map[key];
        moveToHead(node);
        return node->value;
    }

    void put(int key, int value) {
        if (map.count(key)) {
            Node* node = map[key];
            moveToHead(node);
            node->value = value;
        } else {
            Node* node = deleteTail();
            map.erase(node->key);
            delete node;
            node = new Node(key, value);
            addToHead(node);
            map[key] = node;
        }
    }

    void addToHead(Node* node) {
        node->prev = head;
        node->next = head->next;
        head->next->prev = node;
        head->next = node;
    }

    void deleteNode(Node* node) {
        node->next->prev = node->prev;
        node->prev->next = node->next;
    }

    void moveToHead(Node* node) {
        deleteNode(node);
        addToHead(node);
    }

    Node* deleteTail() {
        Node* node = tail->prev;
        deleteNode(node);
    }
};