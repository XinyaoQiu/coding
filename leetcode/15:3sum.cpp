#include "leetcode_base.h"

class Solution {
public:
    vector<vector<int>> threeSum(vector<int>& nums) {
        vector<vector<int>> res;
        sort(nums.begin(), nums.end());
        for (int i = 0; i < nums.size() - 1; i++) {
            int target = -nums[i];
            int left = i + 1;
            int right = nums.size() - 1;
            while (left < right) {
                int sum = nums[left] + nums[right];
                if (sum < target) {
                    left++;
                } else if (sum > target) {
                    right--;
                } else {
                    vector<int> temp = {nums[i], nums[left], nums[right]};
                    res.push_back(temp);
                    while (left < right && nums[left] == temp[1]) {
                        left++;
                    }
                    while (left < right && nums[right] == temp[2]) {
                        right--;
                    }
                }
            }
            while (i + 1 < nums.size() && nums[i + 1] == nums[i]) {
                i++;
            }
        }      
        return res;
    }
};