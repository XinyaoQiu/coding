#include "leetcode_base.h"

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
  public:
    void reverse_dfs(Node* node, Node* new_node, unordered_set<Node*> visited) {
        visited.insert(node);
        for (Node* n : node->neighbors) {
            if (!visited.count(n)) {
                Node* temp = new Node(n->val);
                new_node->neighbors.push_back(temp);
                reverse_dfs(n, temp, visited);
            }
        }
    }

    Node* cloneGraph(Node* node) {
        if (!node) {
            return nullptr;
        }
        Node* new_node = new Node(node->val);
        unordered_set<Node*> visited;
        reverse_dfs(node, new_node, visited);
        return new_node;
    }
};