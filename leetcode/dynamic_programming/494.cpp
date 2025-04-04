#include <numeric>
#include <vector>
using namespace std;

// postive - negative = target
// postive + negative = sum(nums)
// potive = (sum(nums) + target) / 2
// [1, 1, 1, 1, 1], s = 4
// dp[j] = dp[j] + dp[j - nums[i]]

class Solution {
  public:
    int findTargetSumWays(vector<int>& nums, int target) {
        int s = accumulate(nums.begin(), nums.end(), 0) + target;
        if (s < 0 || s % 2 == 1)
            return 0;
        s = s / 2;
        vector<int> dp(s + 1);
        dp[0] = 1;
        int n = nums.size();
        for (int i = 0; i < n; i++) {
            for (int j = s; j >= nums[i]; j--) {
                dp[j] += dp[j - nums[i]];
            }
        }
        return dp[s];
    }
};