#include <vector>
#include <algorithm>
using namespace std;

class Solution {
    vector<vector<int>> edges;
    vector<int> visited;
    vector<int> ans;
    bool is_cycle = false;
    void dfs(int u) {
        visited[u] = 1;
        for (int v : edges[u]) {
            if (visited[v] == 0) {
                visited[v] = 1;
                dfs(v);
                if (is_cycle) {
                    return;
                }
            } else if (visited[v] == 1) {
                is_cycle = true;
                return;
            }
        }
        visited[u] = 2;
        ans.push_back(u);
    }
public:
    vector<int> findOrder(int numCourses, vector<vector<int>>& prerequisites) {
        edges.resize(numCourses);
        visited.resize(numCourses);
        for (auto& v : prerequisites) {
            edges[v[0]].push_back(v[1]);
        }
        for (int i = 0; i < numCourses && !is_cycle; ++i) {
            if (!visited[i]) {
                dfs(i);
            }
        }
        if (is_cycle) {
            return {};
        }
        return ans;
    }
};