#include <vector>
using namespace std;

class Solution {
    vector<vector<int>> edges;
    vector<int> visited;
    bool valid = true;
    void dfs(int u) {
        if (visited[u] == 1) {
            valid = false;
            return;
        }
        if (valid && visited[u] == 0) {
            visited[u] = 1;
            for (int v : edges[u]) {
                dfs(v);
            }
            visited[u] = 2;
        }
    }
public:
    bool canFinish(int numCourses, vector<vector<int>>& prerequisites) {
        edges.resize(numCourses);
        visited.resize(numCourses);
        for (auto& v : prerequisites) {
            edges[v[0]].push_back(v[1]);
        }
        for (int i = 0; i < numCourses; ++i) {
            if (!visited[i]) {
                dfs(i);
            }
        }
        return valid;
    }
};