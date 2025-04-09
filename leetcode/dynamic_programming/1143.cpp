#include <vector>
#include <string>
using namespace std;

class Solution {
    // dp[i][j] = text1[i] == text2[j] ? dp[i - 1][j - 1] + 1 : max(dp[i -
    // 1][j], dp[i][j - 1]);
    //  (i-1, j-1) ;  (i-1, j)
    //  (i, j-1)   ;  (i, j)
  public:
    int longestCommonSubsequence(string text1, string text2) {
        if (text1.size() > text2.size())
            swap(text1, text2);
        int m = text1.size(), n = text2.size();
        vector<int> dp(m + 1);
        int pre = 0;
        for (int i = 0; i < n; i++) {
            pre = 0;
            for (int j = 0; j < m; j++) {
                int tmp = dp[j + 1];
                dp[j + 1] =
                    text1[j] == text2[i] ? pre + 1 : max(dp[j + 1], dp[j]);
                pre = tmp;
            }
        }
        return dp[m];
    }
};