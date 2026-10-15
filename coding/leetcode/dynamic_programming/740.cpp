#include <vector>
#include <unordered_map>
using namespace std;

class Solution {
  public:
    int deleteAndEarn(vector<int>& nums) {
        unordered_map<int, int> map;
        int n = 0;
        for (int x : nums) {
            map[x] += x;
            n = max(n, x);
        }
        // dp[i] = max(dp[i - 2] + map[i], dp[i - 1])
        int p = 0, q = 0;
        for (int i = 1; i <= n; i++) {
            int r = max(p + map[i], q);
            p = q;
            q = r;
        }
        return q;
    }
};