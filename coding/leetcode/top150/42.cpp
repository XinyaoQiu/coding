#include <vector>
using namespace std;

class Solution {
  public:
    int trap(vector<int>& height) {
        int n = height.size();
        int i = 0, j = n - 1;
        int i_max = 0, j_max = 0, ans = 0;
        while (i < j) {
            i_max = max(i_max, height[i]);
            j_max = max(j_max, height[j]);
            if (i_max < j_max) {
                ans += i_max - height[i];
                i++;
            } else {
                ans += j_max - height[j];
                j--;
            }
        }
        return ans;
    }
};