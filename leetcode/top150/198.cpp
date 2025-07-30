#include <vector>
using namespace std;

class Solution {
    // [0, 0, 1, 2, 4, 4]
public:
    int rob(vector<int>& nums) {
        int p = 0, q = 0;
        for (int n : nums) {
            int r = max(p + n, q);
            p = q;
            q = r;
        }
        return q;
    }
};