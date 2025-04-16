#include <vector>
#include <algorithm>
using namespace std;

// [[1, 6], [2, 8], [7, 12], [10, 16]]
// y = 6, 12

// [[1, 2], [3, 4], [5, 6], [7, 8]]
// y = 2, 4, 6, 8

class Solution {
  public:
    int findMinArrowShots(vector<vector<int>>& points) {
        int n = points.size();
        sort(points.begin(), points.end(), [](vector<int>& v1, vector<int>& v2) {
            return v1[1] < v2[1];
        });
        int pos = points[0][1], ans = 1;
        for (auto& v : points) {
            if (v[0] > pos) {
                pos = v[1];
                ans++;
            }
        }
        return ans;
    }
};