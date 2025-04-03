#include <string>
#include <vector>
using namespace std;

// 22233
// [(1), 1, 2, 4, 4, 8]

class Solution {
    const int MOD = 1e9 + 7;

  public:
    int countTexts(string pressedKeys) {
        int n = pressedKeys.size();
        vector<int> dp(n + 1);
        dp[0] = 1;
        for (int i = 1; i <= n; i++) {
            char key = pressedKeys[i - 1];
            dp[i] = dp[i - 1];
            if (i >= 2 && pressedKeys[i - 2] == key) {
                dp[i] = (dp[i] + dp[i - 2]) % MOD;
            }
            if (i >= 3 && pressedKeys[i - 2] == key &&
                pressedKeys[i - 3] == key) {
                dp[i] = (dp[i] + dp[i - 3]) % MOD;
            }
            if (key == '7' || key == '9') {
                if (i >= 4 && pressedKeys[i - 2] == key &&
                    pressedKeys[i - 3] == key && pressedKeys[i - 4] == key) {
                    dp[i] = (dp[i] + dp[i - 4]) % MOD;
                }
            }
        }
        return dp[n] % MOD;
    }
};