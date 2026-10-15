#include <vector>
using namespace std;

class Solution {
    const int MOD = 1e9 + 7;
  public:
    int countGoodStrings(int low, int high, int zero, int one) {
        vector<int> dp(high + 1);
        dp[0] = 1;
        int ans = 0;
        for (int i = 1; i <= high; i++) {
            if (i >= zero) dp[i] = (dp[i] + dp[i - zero]) % MOD;
            if (i >= one) dp[i] = (dp[i] + dp[i - one]) % MOD;
            if (i >= low) ans = (ans + dp[i]) % MOD;
        }
        return ans;
    }
};