#include <vector>
using namespace std;

class Solution {
    vector<vector<int>> ans;
    vector<int> path;
    void dfs(vector<int>& candidates, int target, int i) {
        if (target == 0) {
            ans.emplace_back(path);
            return;
        }
        if (target > 0) {
            for (int j = i; j < candidates.size(); ++j) {
                path.push_back(candidates[j]);
                dfs(candidates, target - candidates[j], j);
                path.pop_back();
            }
        }
    }
public:
    vector<vector<int>> combinationSum(vector<int>& candidates, int target) {
        dfs(candidates, target, 0);
        return ans;
    }
};