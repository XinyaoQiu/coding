#include <vector>
using namespace std;

// init dp = all -1, dp[0] = 0;
// dp[i][j] = max(dp[i-1][j], dp[i-1][j-nums[i]] + 1)

class Solution {
  public:
    int lengthOfLongestSubsequence(vector<int>& nums, int target) {
        int n = nums.size(), s = 0;
        vector<int> dp(target + 1, -1);
        dp[0] = 0;
        for (int x : nums) {
            s = min(s + x, target);
            for (int j = s; j >= x; j--) {
                if (dp[j - x] != -1 && dp[j - x] + 1 > dp[j]) {
                    dp[j] = dp[j - x] + 1;
                }
            }
        }
        return dp[target];
    }
};