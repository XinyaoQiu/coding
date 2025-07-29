class Solution {
    // -1 -> 0, 0 -> 1, 1 -> 1, 2 -> 2, 3 -> 3
public:
    int climbStairs(int n) {
        int p = 0, q = 1;
        for (int i = 0; i < n; ++i) {
            int r = p + q;
            p = q;
            q = r;
        }
        return q;
    }
};