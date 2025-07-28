#include <algorithm>
class Solution {
public:
    int mySqrt(int x) {
        int L = 0, R = x;
        while (L <= R) {
            int M = L + (R - L) / 2;
            if ((long long)M * M <= x) {
                L = M + 1;
            } else {
                R = M - 1;
            }
        }
        return R;
    }
};