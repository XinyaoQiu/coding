#include <string>
#include <vector>
#include <numeric>
using namespace std;

class Solution {
  public:
    int minDistance(string word1, string word2) {
        if (word1.size() < word2.size()) swap(word1, word2);
        int m = word1.size(), n = word2.size();
        vector<int> dp(n + 1);
        iota(dp.begin(), dp.end(), 0);
        for (int i = 1; i <= m; i++) {
            int pre = i - 1;
            dp[0] = i;
            for (int j = 1; j <= n; j++) {
                int tmp = dp[j];
                dp[j] = word1[i-1] == word2[j-1] ? pre : min(dp[j], dp[j-1]) + 1;
                pre = tmp;
            }
        }
        return dp[n];
    }
};