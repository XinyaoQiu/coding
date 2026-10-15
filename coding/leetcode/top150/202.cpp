#include <unordered_set>
#include <vector>
using namespace std;

class Solution {
    int get_next(int n) {
        int res = 0;
        while (n > 0) {
            res += (n % 10) * (n % 10);
            n /= 10;
        }
        return res;
    }
  public:
    bool isHappy(int n) {
        int slow = n, fast = n;
        while (fast != 1) {
            slow = get_next(slow);
            fast = get_next(get_next(fast));
            if (slow == fast) {
                break;
            }
        }
        return fast == 1;
    }
};