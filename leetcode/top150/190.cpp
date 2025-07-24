#include <vector>
using namespace std;

class Solution {
public:
    int reverseBits(int n) {
        int ans = 0, pow = 31;
        while (n > 0) {
            ans += (n % 2) * (1 << pow);
            --pow;
            n /= 2;
        }
        return ans;
    }
};