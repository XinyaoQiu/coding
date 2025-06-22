#include <vector>
#include <string>
#include <unordered_map>
using namespace std;

class Solution {
    class UnionFind {
        vector<int> parents;
        vector<double> weights;
    public:
        UnionFind(int n) : parents(n), weights(n, 1.0) {
            for (int i = 0; i < n; ++i) {
                parents[i] = i;
            }
        }
        int find(int x) {
            if (x == parents[x]) {
                return x;
            }
            int p = parents[x];
            parents[x] = find(p);
            weights[x] *= weights[p];
            return parents[x];
        }
        void unite(int x, int y, double w) {
            int rootx = find(x);
            int rooty = find(y);
            if (rootx == rooty) {
                return;
            }
            if (rootx < rooty) {
                parents[rootx] = rooty;
                weights[rootx] = w * weights[y] / weights[x];
            } else {
                parents[rooty] = rootx;
                weights[rooty] = weights[x] / (weights[y] * w);
            }
        }
        double is_connected(int x, int y) {
            int rootx = find(x);
            int rooty = find(y);
            if (rootx == rooty) {
                return weights[x] / weights[y];
            }
            return -1.0;
        }
    };
public:
    vector<double> calcEquation(vector<vector<string>>& equations, vector<double>& values, vector<vector<string>>& queries) {
        unordered_map<string, int> m;
        int n = 0;
        for (auto& equation : equations) {
            if (!m.count(equation[0])) {
                m[equation[0]] = n;
                ++n;
            }
            if (!m.count(equation[1])) {
                m[equation[1]] = n;
                ++n;
            }
        }
        UnionFind uf(n);
        for (int i = 0; i < equations.size(); ++i) {
            uf.unite(m[equations[i][0]], m[equations[i][1]], values[i]);
        }
        vector<double> ans;
        for (auto& q : queries) {
            if (!m.count(q[0]) || !m.count(q[1])) {
                ans.push_back(-1.0);
            } else {
                ans.push_back(uf.is_connected(m[q[0]], m[q[1]]));
            }
        }
        return ans;
    }
};