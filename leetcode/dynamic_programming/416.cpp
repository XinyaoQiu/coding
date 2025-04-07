#include <vector>
using namespace std;

// subset sum equal to sum(nums) / 2
// init dp to all false, dp[0] = true
// dp[i][j] = dp[i - 1][j] || dp[i - 1][j - nums[i]]

class Solution {
  public:
    bool canPartition(vector<int>& nums) {
        int s = 0;
        for (int x : nums) s += x;
        if (s % 2 == 1) return false;
        s = s / 2;
        vector<int> dp(s + 1);
        dp[0] = 1;
        for (int x : nums) {
            for (int j = s; j >= x; j--) {
                dp[j] = dp[j] || dp[j - x];
            }
        }
        return dp[s];
    }
};