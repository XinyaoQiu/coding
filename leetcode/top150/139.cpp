#include <vector>
#include <string>
using namespace std;

class Solution {
    // dp = [1, 0, 0, 0, 0]
    // s = ['a', 'a', 'a', 'a']
public:
    bool wordBreak(string s, vector<string>& wordDict) {
        int n = s.size();
        vector<int> dp(n + 1, 0);
        dp[0] = 1;
        for (int i = 0; i < n; ++i) {
            for (string& word : wordDict) {
                int wordLen = word.size();
                if (i + 1 < wordLen) {
                    continue;
                }
                if (dp[i + 1]) {
                    break;
                }
                if (s.substr(i - wordLen + 1, wordLen) == word) {
                    dp[i + 1] = dp[i + 1] || dp[i - wordLen + 1];
                }
            }
        }
        return dp[n];
    }
};