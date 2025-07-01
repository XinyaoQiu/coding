#include <vector>
using namespace std;

class Solution {
    vector<vector<int>> ans;
    vector<int> path;
    void dfs(int n, int k, int i) {
        if (path.size() == k) {
            ans.emplace_back(path);
            return;
        }
        for (int j = i; j < n; ++j) {
            path.push_back(j + 1);
            dfs(n, k, j + 1);
            path.pop_back();
        }
    }
public:
    vector<vector<int>> combine(int n, int k) {
        dfs(n, k, 0);
        return ans;
    }
};