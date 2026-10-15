#include "leetcode_base.h"

class Solution {
public:
    int maxSubArray(vector<int>& nums) {
        int largest = nums[0];
        int result = largest;
        for (int i = 1; i < nums.size(); i++) {
            largest = max(largest + nums[i], nums[i]);
            result = max(result, largest);
        }
        return result;
    }
};