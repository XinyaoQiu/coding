#include <vector>
#include <string>
#include <unordered_map>
using namespace std;

class Solution {
    class UnionFind {
        vector<int> parent;
        vector<double> weight;
    public:
        UnionFind(int n) : parent(n), weight(n, 1.0) {
            for (int i = 0; i < n; i++) {
                parent[i] = i;
            }
        }
        int find(int x) {
            if (x == parent[x]) return x;
            int p = parent[x];
            parent[x] = find(p);
            weight[x] *= weight[p];
        }
        void unite(int x, int y, double w) {
            int parent_x = find(x);
            int parent_y = find(y);
            /*   a / b = x / y * y / b / (x / a)
                      a--------b
                     /        /
                    /        /
                   x--------y 
            */
            if (parent_x == parent_y) return;
            parent[parent_x] = parent_y;
            weight[parent_x] = w * weight[y] / weight[x];
        }
        double isConnected(int x, int y) {
            int parent_x = find(x);
            int parent_y = find(y);
            if (parent_x == parent_y) {
                return weight[x] / weight[y];
            }
            return -1.0;
        }
    };
public:
    vector<double> calcEquation(vector<vector<string>>& equations, vector<double>& values, vector<vector<string>>& queries) {
        unordered_map<string, int> mp;
        int count = 0;
        for (auto& v : equations) {
            if (!mp.count(v[0])) {
                mp[v[0]] = count++;
            }
            if (!mp.count(v[1])) {
                mp[v[1]] = count++;
            }
        }
        UnionFind uf(count);
        for (int i = 0; i < equations.size(); i++) {
            auto& v = equations[i];
            uf.unite(mp[v[0]], mp[v[1]], values[i]);
        }
        vector<double> ans;
        for (auto& q : queries) {
            if (!mp.count(q[0]) || !mp.count(q[1])) {
                ans.emplace_back(-1.0);
            } else {
                ans.emplace_back(uf.isConnected(mp[q[0]], mp[q[1]]));
            }
        }
        return ans;
    }
};