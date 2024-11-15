#include "leetcode_base.h"

class Solution {
public:
    int minSubArrayLen(int target, vector<int>& nums) {
        int left = 0, right = 0;
        int sum = 0;
        int res = INT_MAX;
        while (right < nums.size()) {
            sum += nums[right];
            while (sum >= target) {
                res = min(res, right - left + 1);
                sum -= nums[left];
                left++;
            }
            right++;
        }
        return res == INT_MAX ? 0 : res;
    }
};