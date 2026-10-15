#include <iostream>
#include <vector>
#include <unordered_map>
using namespace std;

class Solution {
    vector<int> path;
    int ans = 0;
    unordered_map<int, int> diag;
    unordered_map<int, int> anti_diag;
    void dfs(int n, int i) {
        if (i == n) {
            for (int i = 0; i < path.size(); ++i) {
                cout << path[i] << " ";
            }
            cout << endl;
            ans++;
            return;
        }
        for (int j = i; j < n; ++j) {
            if (!diag[i - path[j]] && !anti_diag[i + path[j]]) {
                diag[i - path[j]] = anti_diag[i + path[j]] = 1;
                swap(path[i], path[j]);
                dfs(n, i + 1);
                swap(path[i], path[j]);
                diag[i - path[j]] = anti_diag[i + path[j]] = 0;
            }
        }
    }
public:
    int totalNQueens(int n) {
        for (int i = 0; i < n; ++i) {
            path.push_back(i);
        }
        dfs(n, 0);
        return ans;
    }
};