#include <vector>
#include <unordered_map>
using namespace std;

class Solution {
public:
    int numberOfSubarrays(vector<int>& nums, int k) {
        vector<int> cnt(nums.size() + 1, 0);
        int odd = 0, ans = 0;
        cnt[0] = 1;
        for (int n : nums) {
            odd += (n % 2);
            cnt[odd]++;
            if (odd >= k) {
                ans += cnt[odd - k];
            }
        }
        return ans;
    }
};