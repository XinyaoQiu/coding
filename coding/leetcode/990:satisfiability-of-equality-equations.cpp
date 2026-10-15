#include <leetcode_base.h>

class Solution {
  public:
    class UnionFind {
      public:
        vector<int> fa;

        UnionFind(int n) {
            for (int i = 0; i < n; i++) {
                fa.push_back(i);
            }
        }

        int Find(int x) {
            while (x != fa[x]) {
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

        bool is_connected(int x, int y) { return Find(x) == Find(y); }
    };

    bool equationsPossible(vector<string>& equations) {
        UnionFind uf(26);
        for (string e : equations) {
            if (e[1] == '=') {
                uf.Union(e[0] - 'a', e[3] - 'a');
            }
        }
        for (string e : equations) {
            if (e[1] == '!' && uf.is_connected(e[0] - 'a', e[3] - 'a')) {
                return false;
            }
        }
        return true;
    }
};