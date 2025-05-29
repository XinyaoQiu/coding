#include <vector>
#include <unordered_set>
#include <climits>
using namespace std;

class Solution {
  public:
    int longestConsecutive(vector<int>& nums) {
        unordered_set<int> s;
        for (int n : nums) {
            s.insert(n);
        }
        int ans = 0;
        for (int n : s) {
            if (s.count(n - 1)) {
                continue;
            }
            int i = n + 1;
            while (s.count(i)) {
                ++i;
            }
            ans = max(ans, i - n);
        }
        return ans;
    }
};