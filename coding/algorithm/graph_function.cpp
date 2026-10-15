#include "graph_structure.h"
#include <deque>
#include <iostream>
#include <unordered_map>
#include <vector>

using namespace std;

void dfs(Node* node) {
    if (!node)
        return;
    vector<Node*> stack;
    unordered_set<Node*> visited;
    stack.push_back(node);
    visited.insert(node);
    cout << node->value << " ";
    while (!stack.empty()) {
        Node* cur = stack.back();
        stack.pop_back();
        for (Node* n : cur->nexts) {
            if (!visited.count(n)) {
                visited.insert(n);
                stack.push_back(cur);
                stack.push_back(n);
                cout << n->value << " ";
                break;
            }
        }
    }
    cout << endl;
}

void dfs_reverse(Node* node, unordered_set<Node*>& visited) {
    if (!node) {
        return;
    }
    if (!visited.count(node)) {
        visited.insert(node);
        for (Node* n : node->nexts) {
            dfs_reverse(n, visited);
        }
    }
}

void bfs(Node* node) {
    if (!node)
        return;
    deque<Node*> queue;
    unordered_set<Node*> visited;
    queue.push_front(node);
    visited.insert(node);
    cout << node->value << " ";
    while (!queue.empty()) {
        Node* cur = queue.back();
        queue.pop_back();
        for (Node* n : cur->nexts) {
            if (!visited.count(n)) {
                queue.push_front(n);
                visited.insert(n);
                cout << n->value << " ";
            }
        }
    }
    cout << endl;
}

Node* getMinAndUnselected(unordered_map<Node*, int>& distanceMap,
                          unordered_set<Node*>& selected) {
    Node* minNode = nullptr;
    int minDistance = INT32_MAX;
    for (auto& [key, value] : distanceMap) {
        
    }
    return minNode;
}

void dijkstra(Node* head) {}

int main() {
    Graph* g = new Graph();
    Node* head = nullptr;
    int n;
    cin >> n;
    for (int i = 0; i < n; i++) {
        int u, v;
        cin >> u >> v;
        Node* node_u = g->nodes.count(u) ? g->nodes[u] : new Node(u);
        Node* node_v = g->nodes.count(v) ? g->nodes[v] : new Node(v);
        if (head == nullptr) {
            head = node_u;
        }
        node_u->nexts.push_back(node_v);
        node_v->nexts.push_back(node_u);
        g->nodes[u] = node_u;
        g->nodes[v] = node_v;
    }
    dfs(head);
}