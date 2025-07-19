#include <vector>
using namespace std;

class Solution {
    int lower_bound(vector<int>& nums, int target) {
        int L = 0, R = nums.size() - 1;
        while (L <= R) {
            int M = L + (R - L) / 2;
            if (nums[M] >= target) {
                R = M - 1;
            } else {
                L = M + 1;
            } 
        }
        return L;
    }
public:
    vector<int> searchRange(vector<int>& nums, int target) {
        int start = lower_bound(nums, target);
        if (start == nums.size() || nums[start] != target) {
            return {-1, -1};
        }
        int end = lower_bound(nums, target + 1) - 1;
        return {start, end};
    }
};