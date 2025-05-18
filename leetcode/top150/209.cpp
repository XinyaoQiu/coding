#include <vector>
using namespace std;

class Solution {
  public:
    int minSubArrayLen(int target, vector<int>& nums) {
        int res = nums.size() + 1, curr = 0;
        for (int i = 0, j = 0; j < nums.size(); j++) {
            curr += nums[j];
            while (curr >= target) {
                res = min(res, j - i + 1);
                curr -= nums[i++];
            }
        }
        return res > nums.size() ? 0 : res;
    }
};