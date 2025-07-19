#include <vector>
using namespace std;

class Solution {
public:
    int search(vector<int>& nums, int target) {
        int L = 0, R = nums.size() - 2;
        while (L <= R) {
            int M = L + (R - L) / 2;
            if (target > nums.back()) {
                if (nums[M] < target && nums[M] > nums.back()) {
                    L = M + 1;
                } else {
                    R = M - 1;
                }
            } else {
                if (nums[M] >= target && nums[M] <= nums.back()) {
                    R = M - 1;
                } else {
                    L = M + 1;
                }
            }
        }
        return nums[L] == target ? L : -1;
    }
};