#include <vector>
using namespace std;

class Solution {
public:
    int findMin(vector<int>& nums) {
        int L = 0, R = nums.size() - 2;
        while (L <= R) {
            int M = L + (R - L) / 2;
            if (nums[M] > nums.back()) {
                L = M + 1;
            } else {
                R = M - 1;
            }
        }
        return nums[L];
    }
};