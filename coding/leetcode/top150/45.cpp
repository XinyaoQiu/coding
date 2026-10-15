#include <vector>
#include <climits>
using namespace std;

class Solution {
  public:
    int jump(vector<int>& nums) {
        int start = 0, far = 0, step = 0;
        while (far < nums.size() - 1) { 
            int next = far;
            for (int i = start; i <= far; i++) {
                next = max(next, i + nums[i]);
            }
            start = far + 1;
            far = next;
            step++;
        }
        return step;
    }
};