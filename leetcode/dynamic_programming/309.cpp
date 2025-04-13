#include <vector>
#include <climits>
using namespace std;

// prices = [1, 2, 3, 0, 2]
// dp[0][true] = -inf
// dp[0][false] = 0
// dp[-1][false] = 0
// dp[i+1][true] = max(dp[i][true], dp[i-1][false] - prices[i])
// dp[i+1][false] = max(dp[i][false], dp[i][true] + prices[i])
class Solution {
  public:
    int maxProfit(vector<int>& prices) {
        int p = INT_MIN, q = 0, r = 0;
        for (int price : prices) {
            int new_q = max(q, p + price);
            p = max(p, r - price);
            r = q;
            q = new_q;
        }
        return q;
    }
};