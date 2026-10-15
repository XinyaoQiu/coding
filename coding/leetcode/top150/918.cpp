#include <vector>
using namespace std;

class Solution {
public:
    int maxSubarraySumCircular(vector<int>& nums) {
        int sum = 0, currMax = 0, currMin = 0, maxi = INT_MIN, mini = INT_MAX;
        for (int n : nums) {
            sum += n;
            currMax = max(currMax + n, n);
            currMin = min(currMin + n, n);
            maxi = max(currMax, maxi);
            mini = min(currMin, mini);
        }
        if (mini == sum) {
            return maxi;
        }
        return max(maxi, sum - mini);
    }
};