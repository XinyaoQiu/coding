#include <vector>
#include <climits>
using namespace std;

class Solution {
public:
    int maxSubArray(vector<int>& nums) {
        int curr = 0, ans = INT_MIN;
        for (int n : nums) {
            curr = max(curr + n, n);
            ans = max(curr, ans);
        }
        return ans;
    }
};