#include <vector>
using namespace std;

class Solution {
public:
    int minimumTotal(vector<vector<int>>& triangle) {
        vector<vector<int>> dp;
        dp.emplace_back(triangle[0]);
        for (int i = 1; i < triangle.size(); ++i) {
            vector<int> curr({dp[i - 1][0] + triangle[i][0]});
            for (int j = 1; j < triangle[i].size() - 1; ++j) {
                curr.push_back(min(dp[i - 1][j - 1], dp[i - 1][j]) + triangle[i][j]);
            }
            curr.push_back(dp[i - 1].back() + triangle[i].back());
            dp.push_back(curr);
        }
        int ans = INT_MAX;
        for (int n : dp[triangle.size() - 1]) {
            ans = min(ans, n);
        }
        return ans;
    }
};