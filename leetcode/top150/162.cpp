#include <vector>
using namespace std;

class Solution {
public:
    int findPeakElement(vector<int>& nums) {
        int L = 0, R = nums.size() - 2;
        while (L <= R) {
            int M = L + (R - L) / 2;
            if (nums[M] > nums[M + 1]) {
                R = M - 1;
            } else {
                L = M + 1;
            }
        }
        return L;
    }
};