class Solution {
    double quickPow(double x, long long n) {
        double ans = 1.0, y = x;
        while (n > 0) {
            if (n % 2 == 1) {
                ans *= y;
            }
            y *= y;
            n /= 2;
        }
        return ans;
    }
public:
    double myPow(double x, long long n) {
        return n >= 0 ? quickPow(x, n) : 1.0 / quickPow(x, -n);
    }
};