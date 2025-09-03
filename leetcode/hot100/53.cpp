#include <vector>
#include <climits>
using namespace std;

class Solution {
public:
    int maxSubArray(vector<int>& nums) {
        int pre = 0, sum = 0, ans = INT_MIN;
        for (int n : nums) {
            sum += n;
            ans = max(ans, sum - pre);
            pre = min(pre, sum);
        }
        return ans;
    }
};