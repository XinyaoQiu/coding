#include <vector>
using namespace std;

// nums = [1, 4, 9]
// init dp to all 0
// dp[i][j] = dp[i - 1][j] + dp[i - 1][j - nums[i]]

class Solution {
    const int MOD = 1e9 + 7;
    long long fastPow(int n, int x) {
        long long result = 1;
        while (x > 0) {
            if (x % 2 == 1) result *= n;
            n *= n;
            x /= 2;
        }
        return result;
    }
  public:
    int numberOfWays(int n, int x) {
        vector<int> dp(n + 1, 0);
        dp[0] = 1;
        for (int i = 1; i <= n; i++) {
            int pow = fastPow(i, x);
            if (pow > n) break;
            for (int j = n; j >= pow; j--) {
                dp[j] = (dp[j] + dp[j - pow]) % MOD;
            }
        }
        return dp[n];
    }
};