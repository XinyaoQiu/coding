#include <iostream>
#include <unordered_set>
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

void backtrack(vector<string>& ans, string& path,
               vector<vector<string>>& tuples, unordered_map<string, int>& m,
               int i) {
    if (i == path.size()) {
        for (auto& tuple : tuples) {
            if (!(m[tuple[0]] < m[tuple[1]] && m[tuple[1]] < m[tuple[2]])) {
                return;
            }
        }
        ans.push_back(path);
        return;
    }
    if (ans.size() == 1) {
        return;
    }
    for (int j = i; j < path.size(); ++j) {
        swap(path[i], path[j]);
        m[path.substr(i, 1)] = i;
        backtrack(ans, path, tuples, m, i + 1);
        swap(path[i], path[j]);
        m.erase(path.substr(i, 1));
    }
}

string process(int n, vector<vector<string>> tuples) {
    unordered_set<string> s;
    for (auto& v : tuples) {
        for (auto& c : v) {
            s.insert(c);
        }
    }
    string path;
    for (auto& c : s) {
        path += c;
    }
    unordered_map<string, int> m;
    vector<string> ans;
    backtrack(ans, path, tuples, m, 0);
    return ans[0];
}

int main() {
    vector<vector<string>> tuples = {{"a", "b", "c"},
                                     {"b", "c", "e"},
                                     {"c", "d", "f"},
                                     {"b", "g", "c"},
                                     {"e", "d", "f"}};
    cout << process(7, tuples) << endl;
}