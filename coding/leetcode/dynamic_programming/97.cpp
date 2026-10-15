#include <string>
#include <vector>
using namespace std;

class Solution {
  public:
    bool isInterleave(string s1, string s2, string s3) {
        int m = s1.size(), n = s2.size(), t = s3.size();
        if (m + n != t) return false;
        vector<int> dp(n + 1);
        dp[0] = true;
        for (int i = 1; i <= n; i++) {
            dp[i] = dp[i-1] && s2[i-1] == s3[i-1];
        }
        for (int i = 1; i <= m; i++) {
            dp[0] = dp[0] && s1[i-1] == s3[i-1];
            for (int j = 1; j <= n; j++) {
                dp[j] = dp[j] && s1[i-1] == s3[i+j-1] || dp[j-1] && s2[j-1] == s3[i+j-1];
            }
        }
        return dp[n];
    }
};