#include <vector>
#include <string>
#include <unordered_set>
using namespace std;

class Solution {
    class UnionFind {
        vector<int> parents;
        vector<int> sizes;
    public:
        UnionFind(int n) {
            for (int i = 0; i < n; ++i) {
                parents.push_back(i);
            }
            sizes.resize(n, 1);
        }
        int find(int x) {
            while (x != parents[x]) {
                parents[x] = parents[parents[x]];
                x = parents[x];
            }
            return x;
        }
        void unite(int x, int y) {
            int root_x = find(x), root_y = find(y);
            if (root_x == root_y) {
                return;
            }
            parents[root_x] = root_y;
            sizes[root_y] += sizes[root_x];
            sizes[root_x] = 0;
        }

        int size(int x) {
            return sizes[find(x)];
        }
    };
    bool valid(int n, int i, int j) {
        return i >= 0 && i < n && j >= 0 && j < n;
    }
    const int d[5] = {0, 1, 0, -1, 0};
public:
    int largestIsland(vector<vector<int>>& grid) {
        int n = grid.size();
        UnionFind uf(n * n);
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (grid[i][j] == 1) {
                    if (i < n - 1 && grid[i + 1][j] == 1) {
                        uf.unite(i * n + j, i * n + n + j);
                    }
                    if (j < n - 1 && grid[i][j + 1] == 1) {
                        uf.unite(i * n + j, i * n + j + 1);
                    }
                }
            }
        }
        int ans = 0;
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (grid[i][j] == 0) {
                    int curr = 1;
                    unordered_set<int> s;
                    for (int k = 0; k < 4; ++k) {
                        int x = i + d[k], y = j + d[k + 1];
                        if (!valid(n, x, y) || grid[x][y] == 0) {
                            continue;
                        }
                        int root = uf.find(x * n + y);
                        if (s.count(root) == 0) {
                            curr += uf.size(root);
                            s.insert(root);
                        }
                    }
                    ans = max(ans, curr);
                }
            }
        }
        return ans == 0 ? n * n : ans;
    }
};