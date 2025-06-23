#include <vector>
#include <algorithm>
using namespace std;

class Solution {
    vector<vector<int>> edges;
    vector<int> visited;
    vector<int> ans;
    bool valid = true;
    void dfs(int u) {
        if (!valid || visited[u] == 1) {
            valid = false;
            ans.clear();
            return;
        }
        if (visited[u] == 0) {
            visited[u] = 1;
            for (int v : edges[u]) {
                dfs(v);
                if (!valid) {
                    return;
                }
            }
            visited[u] = 2;
            ans.push_back(u);
        }
    }
public:
    vector<int> findOrder(int numCourses, vector<vector<int>>& prerequisites) {
        edges.resize(numCourses);
        visited.resize(numCourses);
        for (auto& v : prerequisites) {
            edges[v[1]].push_back(v[0]);
        }
        for (int i = 0; i < numCourses && valid; ++i) {
            dfs(i);
        }
        reverse(ans.begin(), ans.end());
        return ans;
    }
};