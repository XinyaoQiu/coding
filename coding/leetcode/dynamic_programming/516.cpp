#include <vector>
#include <string>
#include <climits>
using namespace std;

// dp[i][i] = 1
// dp[i][j] = s[i] == s[j] ? dp[i+1][j-1] + 2 : max(dp[i][j-1], dp[i+1][j])

class Solution {
  public:
    int longestPalindromeSubseq(string s) {
        int n = s.size();
        vector<int> dp(n);
        for (int i = n - 1; i >= 0; i--) {
            int pre = 0;
            dp[i] = 1;
            for (int j = i + 1; j < n; j++) {
                int tmp = dp[j];
                dp[j] = s[i] == s[j] ? pre + 2 : max(dp[j], dp[j-1]);
                pre = tmp;
            }
        }
        return dp[n-1];
    }
};