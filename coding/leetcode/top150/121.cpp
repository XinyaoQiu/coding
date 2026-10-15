#include <vector>
#include <climits>
using namespace std;

class Solution {
  public:
    int maxProfit(vector<int>& prices) {
        int start = INT_MAX, ans = 0;
        for (int price : prices) {
            start = min(start, price);
            ans = max(ans, price - start);
        }
        return ans;
    } 
};