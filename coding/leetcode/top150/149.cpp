#include <vector>
#include <unordered_map>
using namespace std;

class Solution {
    int gcd(int a, int b) {
        return b ? gcd(b, a % b) : a;
    }
public:
    int maxPoints(vector<vector<int>>& points) {
        int n = points.size(), ans = 0;
        if (n <= 2) {
            return n;
        }
        for (int i = 0; i < n; ++i) {
            if (ans > n / 2 || ans >= n - i) {
                return ans;
            }
            int x0 = points[i][0], y0 = points[i][1];
            unordered_map<int, int> m;
            for (int j = i + 1; j < n; ++j) {
                int x = points[j][0], y = points[j][1];
                int del_x = 0, del_y = 0;
                if (x == x0) {
                    del_y = 1;
                } else if (y == y0) {
                    del_x = 1;
                } else {
                    del_x = (x - x0) / gcd(x - x0, y - y0);
                    del_y = (y - y0) / gcd(x - x0, y - y0);
                    if (del_x < 0) {
                        del_x = -del_x;
                        del_y = -del_y;
                    }
                }
                m[del_x + (2e4 + 1) * del_y]++;
                ans = max(ans, m[del_x + (2e4 + 1) * del_y] + 1);
            }
        }
        return ans;
    }
};