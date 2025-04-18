#include <vector>
using namespace std;

class Solution {
  public:
    int removeDuplicates(vector<int>& nums) {
        int i = 0, n = nums.size();
        for (int j = 0; j < n; j++) {
            if (i > 1 && nums[j] == nums[i - 2]) {
                continue;
            }
            nums[i++] = nums[j];
        }
        return i;
    }
};