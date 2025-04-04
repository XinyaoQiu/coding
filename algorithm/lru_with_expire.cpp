#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>

using namespace std;
using namespace std::chrono;

struct Node {
    int key, val;
    Node *prev, *next;
    long long start, duration;
    Node() : key(0), val(0), start(0), duration(0), next(nullptr) {}
    Node(int key_, int val_, long long start_, long long duration_) 
        : key(key_), val(val_), start(start_), duration(duration_), next(nullptr) {}
};

class LRUCache {
    public:
    int capacity;
    unordered_map<int, Node*> cache;
    Node *head, *tail;
  
    long long getCurrentTime() {
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    void removeNode(Node* node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    void addToHead(Node* node) {
        node->next = head->next;
        node->prev = head;
        head->next->prev = node;
        head->next = node;
    }

    LRUCache(int capacity_) : capacity(capacity_) {
        head = new Node();
        tail = new Node();
        head->next = tail;
        tail->next = head;
    }
    
    int get(int key) {
        if (!cache.count(key)) {
            return -1;
        }
        long long currentTime = getCurrentTime();
        Node* node = cache[key];
        if (node->start + node->duration < currentTime) {
            removeNode(node);
            cache.erase(node->key);
            delete node;
            return -1;
        }
        node->start = currentTime;
        removeNode(node);
        addToHead(node);
        return node->val;
    }

    void put(int key, int val, long long duration=-1) {
        long long currentTime = getCurrentTime();
        if (cache.count(key)) {
            Node* node = cache[key];
            node->start = currentTime;
            if (duration >= 0) {
                node->duration = duration;
            }
            node->val = val;
            removeNode(node);
            addToHead(node);
        } else {
            if (cache.size() >= capacity) {
                Node* cur = tail->prev;
                while (cur != head) {
                    if (cur->start + cur->duration < currentTime) {
                        break;
                    }
                }
                if (cur == head) {
                    cur = tail->prev;
                }
                removeNode(cur);
                cache.erase(cur->key);
                delete cur;
            }
            Node* node = new Node(key, val, currentTime, duration);
            addToHead(node);
            cache[key] = node;
        }
    }

    ~LRUCache() {
        delete head;
        delete tail;
    }
};


int main() {
    LRUCache cache(2);
    
    cache.put(1, 100, 2000);  // key=1, 过期时间 2000ms
    cache.put(2, 200, 3000);  // key=2, 过期时间 3000ms
    
    cout << cache.get(1) << endl;  // 输出 100
    
    this_thread::sleep_for(milliseconds(2500));  // 休眠 2.5s
    
    cout << cache.get(1) << endl;  // -1 (过期)
    cout << cache.get(2) << endl;  // 200 (未过期)
    
    return 0;
}
