#include "leetcode_base.h"

class Solution {
public:

    struct UnionFind {
        vector<int> fa;

        UnionFind(int n) {
            for (int i = 0; i < n; i++) {
                fa.push_back(i);
            }
        }

        int Find(int x) {
            while (x != fa[x]) {
                fa[x] = fa[fa[x]];
                x = fa[x];
            }
            return x;
        }

        bool Union(int x, int y) {
            int rx = Find(x);
            int ry = Find(y);
            if (rx == ry) {
                return false;
            }
            fa[rx] = ry;
            return true;
        }

        int SetNum() {
            int count = 0;
            for (int i = 0; i < fa.size(); i++) {
                if (fa[i] == i) {
                    count++;
                }
            }
            return count;
        }
    };

    int findCircleNum(vector<vector<int>>& isConnected) {
        int n = isConnected.size();
        UnionFind uf(n);
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (isConnected[i][j]) {
                    uf.Union(i, j);
                }
            }
        }
        return uf.SetNum();
    }
};