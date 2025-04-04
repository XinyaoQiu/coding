#include "leetcode_base.h"

vector<vector<string>> ans;
vector<bool> on_path;
void backtrack(vector<int>& path, int i) {
    int n = path.size();
    if (i == n) {
        vector<string> tmp;
        for (int i : path) {
            string s(n, '.');
            s[i] = 'Q';
            tmp.push_back(s);
        }
        ans.emplace_back(tmp);
        return;
    }
    for (int j = i; j < n; j++) {
        if (!on_path[i + j + 2 * n] && !on_path[i - j + n]) {
            on_path[i + j + 2 * n] = true;
            on_path[i - j + n] = true;
            swap(path[i], path[j]);
            backtrack(path, i + 1);
            on_path[i + j + 2 * n] = false;
            on_path[i - j + n] = false;
            swap(path[i], path[j]);
        }
    }
}

vector<vector<string>> solveNQueens(int n) {
    vector<int> path(n);
    iota(path.begin(), path.end(), 0);
    on_path = vector<bool>(4 * n, false);
    backtrack(path, 0);
    return ans;
}
int main() {
    auto ans = solveNQueens(4);
    for (auto& v : ans) {
        for (auto& s : v) {
            cout << s << " ";
        }
        cout << endl;
    }
    return 0;
}