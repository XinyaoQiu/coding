#include <iostream>
#include <queue>
#include <vector>
using namespace std;

/*
Eample:
Password contains 4
- b c a
- b c d
- c a d
Password: bcad
Question:
The length is 7
tuples:
{"a", "b", "c"}
{"b", "c", "e"}
{"c", "d", "f"}
{"b", "g", "c"}
{"e", "d", "f"}
answer: abgcedf
*/

string process(int n, vector<vector<string>>& tuples) {
    vector<vector<int>> edges(n);
    vector<int> indegs(n);
    unordered_map<string, int> m;
    vector<string> all;
    for (auto& v : tuples) {
        for (auto& s : v) {
            if (!m.count(s)) {
                int sz = m.size();
                m[s] = sz;
                all.push_back(s);
            }
        }
    }
    for (auto& v : tuples) {
        edges[m[v[0]]].push_back(m[v[1]]);
        edges[m[v[1]]].push_back(m[v[2]]);
        ++indegs[m[v[1]]];
        ++indegs[m[v[2]]];
    }
    queue<int> q;
    for (int i = 0; i < n; ++i) {
        if (indegs[i] == 0) {
            q.push(i);
        }
    }
    string ans;
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        ans += all[u];
        for (int v : edges[u]) {
            if (--indegs[v] == 0) {
                q.push(v);
            }
        }
    }
    return ans.size() == n ? ans : "";
}

int main() {
    vector<vector<string>> tuples = {{"a", "b", "c"},
                                     {"b", "c", "e"},
                                     {"c", "d", "f"},
                                     {"b", "g", "c"},
                                     {"e", "d", "f"}};
    cout << process(7, tuples) << endl;
}