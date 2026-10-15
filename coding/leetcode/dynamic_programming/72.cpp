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
        for (char x : word1) {
            int pre = dp[0];
            dp[0]++;
            for (int j = 1; j <= n; j++) {
                int tmp = dp[j];
                dp[j] = x == word2[j-1] ? pre : min(dp[j], min(dp[j-1], pre)) + 1;
                pre = tmp;
            }
        }
        return dp[n];
    }
};