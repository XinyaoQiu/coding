#include <vector>
using namespace std;

class Solution {
  public:
    int minSubArrayLen(int target, vector<int>& nums) {
        int n = nums.size(), res = n + 1, curr = 0;
        for (int i = 0, j = 0; j < n; j++) {
            curr += nums[j];
            while (curr >= target) {
                res = min(res, j - i + 1);
                curr -= nums[i];
                i++;
            }
        }
        return res > n ? 0 : res;
    }
};