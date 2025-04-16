#include <vector>
#include <unordered_set>
using namespace std;

class Solution {
    int getNext(int n) {
        int res = 0;
        while (n > 0) {
            int d = n % 10;
            res += d * d;
            n /= 10;
        }
        return res;
    }
public:
    bool isHappy(int n) {
        int slow = n, fast = n;
        while (1) {
            slow = getNext(slow);
            fast = getNext(getNext(fast));
            if (slow == fast) {
                break;
            }
        }
        return fast == 1;
    }
};