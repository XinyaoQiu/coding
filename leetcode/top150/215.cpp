#include <vector>
#include <random>
using namespace std;

class Solution {
    int quickSelect(vector<int>& nums, int L, int R, int k) {
        int p1 = L, p2 = L, p3 = R - 1;
        swap(nums[L + rand() % (R - L + 1)], nums[R]);
        while (p2 <= p3) {
            if (nums[p2] < nums[R]) {
                swap(nums[p1++], nums[p2++]);
            } else if (nums[p2] > nums[R]) {
                swap(nums[p2], nums[p3--]);
            } else {
                p2++;
            }
        }
        swap(nums[p2++], nums[R]);
        if (k < p1) {
            return quickSelect(nums, L, p1 - 1, k);
        } else if (k >= p2) {
            return quickSelect(nums, p2, R, k);
        }
        return nums[k];
    }
public:
    int findKthLargest(vector<int>& nums, int k) {
        return quickSelect(nums, 0, nums.size() - 1, nums.size() - k);
    }
};