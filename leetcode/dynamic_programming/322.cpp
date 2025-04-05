#include <vector>
using namespace std;

// reuse dp array
// in-place update

// dp[i][j] = min(dp[i - 1][j], dp[i][j - coins[i]] + 1)
// dp[-1][j] = amount + 1
// dp[i][0] = 0

class Solution {
  public:
    int coinChange(vector<int>& coins, int amount) {
        vector<int> dp(amount + 1, amount + 1);
        dp[0] = 0;
        int n = coins.size();
        for (int i = 0; i < n; i++) {
            for (int j = coins[i]; j <= amount; j++) {
                dp[j] = min(dp[j], dp[j - coins[i]] + 1);
            }
        }
        return dp[amount] == amount + 1 ? -1 : dp[amount];
    }
};