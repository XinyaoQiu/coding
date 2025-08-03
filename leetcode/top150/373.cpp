#include <vector>
#include <queue>
using namespace std;

/*
m = 3, n = 3
i = max(0, 0 - 3 + 1) i = min(3 - 1, 0)
i = max(0, 1 - 3 + 1) i = min(3 - 1, 1)
(0, 0)
(0, 1) (1, 0)
(0, 2) (1, 1) (2, 0)
(1, 2) (2, 1)
(2, 2)
sum = 1; 1 - 
*/


class Solution {
public:
    vector<vector<int>> kSmallestPairs(vector<int>& nums1, vector<int>& nums2, int k) {
        auto cmp = [&nums1, &nums2](pair<int, int>& a, pair<int, int>& b) {
            return nums1[a.first] + nums2[a.second] > nums1[b.first] + nums2[b.second];
        };
        priority_queue<pair<int, int>, vector<pair<int, int>>, decltype(cmp)> pq(cmp);
        vector<vector<int>> ans;
        int m = nums1.size(), n = nums2.size();
        for (int i = 0; i < min(k, m); ++i) {
            pq.emplace(i, 0);
        }
        while (k-- > 0) {
            auto [x, y] = pq.top();
            pq.pop();
            ans.push_back({nums1[x], nums2[y]});
            if (y + 1 < n) {
                pq.emplace(x, y + 1);
            }
        }
        return ans;
    }
};
