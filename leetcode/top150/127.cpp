#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <queue>
#include <climits>
using namespace std;

class Solution {
    unordered_map<string, int> word2id;
    vector<vector<int>> edge;
    int word_num = 0;
    void add_word(string& word) {
        if (!word2id.count(word)) {
            word2id[word] = word_num++;
            edge.emplace_back();
        }
    }
    void add_edge(string& word) {
        add_word(word);
        int id1 = word2id[word];
        for (char& it : word) {
            char tmp = it;
            it = '*';
            add_word(word);
            int id2 = word2id[word];
            edge[id1].push_back(id2);
            edge[id2].push_back(id1);
            it = tmp;
        }
    }
public:
    int ladderLength(string beginWord, string endWord, vector<string>& wordList) {
        for (string& word : wordList) {
            add_edge(word);
        }
        if (!word2id.count(endWord)) {
            return 0;
        }
        add_edge(beginWord);
        queue<int> q({word2id[beginWord]});
        vector<int> visited(word2id.size(), -1);
        visited[word2id[beginWord]] = 0;
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            for (int v : edge[u]) {
                if (visited[v] == -1) {
                    visited[v] = visited[u] + 1;
                    q.push(v);
                }
                if (v == word2id[endWord]) {
                    return visited[v] / 2 + 1;
                }
            }
        }
        return 0;
    }
};