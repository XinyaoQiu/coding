#include <vector>
#include <string>
using namespace std;

class Solution {
    class UnionFind {
        vector<int> parents;
        int count;
    public:
        UnionFind(int n) {
            count = n;
            for (int i = 0; i < n; ++i) {
                parents.push_back(i);
            }
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
            --count;
            parents[root_x] = root_y;
        }

        int connected_count() {
            return count;
        }
    };
public:
    int regionsBySlashes(vector<string>& grid) {
        int n = grid.size();
        UnionFind uf(n * n * 4);
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (grid[i][j] == ' ') {
                    for (int k = 1; k < 4; ++k) {
                        uf.unite((i * n + j) * 4, (i * n + j) * 4 + k);
                    }
                } else if (grid[i][j] == '/') {
                    uf.unite((i * n + j) * 4, (i * n + j) * 4 + 3);
                    uf.unite((i * n + j) * 4 + 1, (i * n + j) * 4 + 2);
                } else if (grid[i][j] == '\\') {
                    uf.unite((i * n + j) * 4, (i * n + j) * 4 + 1);
                    uf.unite((i * n + j) * 4 + 2, (i * n + j) * 4 + 3);
                }
                if (i < n - 1) {
                    uf.unite((i * n + j) * 4 + 2, (i * n + n + j) * 4);
                }
                if (j < n - 1) {
                    uf.unite((i * n + j) * 4 + 1, (i * n + j + 1) * 4 + 3);
                }
            }
        }
        return uf.connected_count();
    }
};