#include <vector>
#include <climits>
using namespace std;

// dp[prices: i+1][times: k][hold: true/false]
// dp[0][0-k][true] = -inf
// dp[0][0-k][false] = 0
// dp[i+1][k+1][true] = max(dp[i][k+1][true], dp[i][k][false] - prices[i])
// dp[i+1][k+1][false] = max(dp[i][k+1][false], dp[i][k+1][true] + prices[i]) 
// dp0 -> false, dp1 -> true
// dp0[k+1] = max(dp0[k+1], dp1[k+1] + price)
// dp1[k+1] = max(dp1[k+1], dp0[k] - price)


class Solution {
  public:
    int maxProfit(int k, vector<int>& prices) {
        vector<int> dp0(k + 1);
        vector<int> dp1(k + 1, INT_MIN);
        for (int price : prices) {
            for (int i = k; i >= 1; i--) {
                dp0[i] = max(dp0[i], dp1[i] + price);
                dp1[i] = max(dp1[i], dp0[i-1] - price);
            }
        }
        return dp0[k];
    }   
};