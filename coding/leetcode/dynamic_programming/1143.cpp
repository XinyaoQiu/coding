#include <string>
#include <vector>
using namespace std;

// init dp to all 0s
// dp[i][j] = text1[i] == text2[j] ? dp[i-1][j-1] + 1 : max(dp[i-1][j], dp[i][j-1])
// pre = dp[j], dp[j] = text1[i] == text2[j] ? pre + 1 : max(dp[j], dp[j-1])

class Solution {
  public:
    int longestCommonSubsequence(string text1, string text2) {
        if (text1.size() < text2.size()) swap(text1, text2);
        int m = text1.size(), n = text2.size();
        vector<int> dp(n + 1);
        for (int i = 1; i <= m; i++) {
            int pre = 0;
            for (int j = 1; j <= n; j++) {
                int tmp = dp[j];
                dp[j] = text1[i-1] == text2[j-1] ? pre + 1 : max(dp[j], dp[j-1]);
                pre = tmp;
            }
        }
        return dp[n];
    }
};