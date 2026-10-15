#include <vector>
using namespace std;

class Solution {
    vector<vector<int>> ans;
    void dfs(vector<int>& nums, int i) {
        if (i == nums.size()) {
            ans.emplace_back(nums);
            return;
        }
        for (int j = i; j < nums.size(); ++j) {
            swap(nums[i], nums[j]);
            dfs(nums, i + 1);
            swap(nums[i], nums[j]);
        }
    }
public:
    vector<vector<int>> permute(vector<int>& nums) {
        dfs(nums, 0);
        return ans;
    }
};