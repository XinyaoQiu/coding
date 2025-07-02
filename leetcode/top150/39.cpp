#include <vector>
using namespace std;

class Solution {
    vector<vector<int>> ans;
    vector<int> path;
    void dfs(vector<int>& candidates, int currSum, int target, int i) {
        if (currSum == target) {
            ans.emplace_back(path);
            return;
        }
        if (currSum < target) {
            for (int j = i; j < candidates.size(); ++j) {
                path.push_back(candidates[j]);
                dfs(candidates, currSum + candidates[j], target, j);
                path.pop_back();
            }
        }
    }
public:
    vector<vector<int>> combinationSum(vector<int>& candidates, int target) {
        dfs(candidates, 0, target, 0);
        return ans;
    }
};