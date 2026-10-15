#include <vector>
#include <climits>
using namespace std;

// dp[i][j] = max([dp[i][k] + dp[k][j] + v[i] * v[j] * v[k] for k in i+1..j-1])
// dp[i][i+1] = 0
// dp[0][n-1], dp[0][1] + dp[1][n-1] + v[0] * v[1] * v[n-1]

class Solution {
  public:
    int minScoreTriangulation(vector<int>& v) {
        int n = v.size();
        vector<vector<int>> dp(n, vector<int>(n));
        for (int i = n - 3; i >= 0; i--) {
            for (int j = i + 2; j < n; j++) {
                int res = INT_MAX;
                for (int k = i + 1; k < j; k++) {
                    res = min(res, dp[i][k] + dp[k][j] + v[i] * v[j] * v[k]);
                }
                dp[i][j] = res;
            }
        }
        return dp[0][n-1];
    }
};