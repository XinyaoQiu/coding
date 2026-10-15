#include <vector>
#include <unordered_map>
#include <unordered_set>

using namespace std;

struct Node;
struct Edge;

struct Edge {
    int weight;
    Node* from;
    Node* to;

    Edge(int weight_) : weight(weight_), from(nullptr), to(nullptr) {}
};

struct Node {
    int value;
    vector<Node*> nexts;
    vector<Edge*> edges;

    Node(int value_) : value(value_) {
        nexts = vector<Node*>();
        edges = vector<Edge*>();
    }
};

struct Graph {
    unordered_map<int, Node*> nodes;
    unordered_set<Edge*> edges;

    Graph() {
        nodes = unordered_map<int, Node*>();
        edges = unordered_set<Edge*>();
    }  
};