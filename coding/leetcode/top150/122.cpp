#include <vector>
#include <climits>
using namespace std;

class Solution {
  public:
    int maxProfit(vector<int>& prices) {
        int pre = INT_MAX, ans = 0;
        for (int price : prices) {
            if (price > pre) {
                ans += price - pre;
            }
            pre = price;
        }
        return ans;
    }
};