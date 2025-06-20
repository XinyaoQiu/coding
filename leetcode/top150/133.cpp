#include <vector>
#include <queue>
#include <unordered_map>
using namespace std;

// Definition for a Node.
class Node {
public:
    int val;
    vector<Node*> neighbors;
    Node() {
        val = 0;
        neighbors = vector<Node*>();
    }
    Node(int _val) {
        val = _val;
        neighbors = vector<Node*>();
    }
    Node(int _val, vector<Node*> _neighbors) {
        val = _val;
        neighbors = _neighbors;
    }
};


class Solution {
    unordered_map<Node*, Node*> m;
public:
    Node* cloneGraph(Node* node) {
        if (!node) {
            return nullptr;
        }
        if (m.count(node)) {
            return m[node];
        }
        Node* copy_node = new Node(node->val);
        m[node] = copy_node;
        for (auto& n : node->neighbors) {
            copy_node->neighbors.emplace_back(cloneGraph(n));
        }
        return copy_node;
    }
};