#include <algorithm>
#include <queue>
#include <vector>
using namespace std;

class Solution {
    vector<vector<int>> edges;
    vector<int> indegs;

  public:
    vector<int> findOrder(int numCourses, vector<vector<int>>& prerequisites) {
        edges.resize(numCourses);
        indegs.resize(numCourses);
        for (auto& v : prerequisites) {
            edges[v[1]].push_back(v[0]);
            ++indegs[v[0]];
        }
        vector<int> ans;
        queue<int> q;
        for (int i = 0; i < numCourses; ++i) {
            if (indegs[i] == 0) {
                q.push(i);
            }
        }
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            ans.push_back(u);
            for (int v : edges[u]) {
                --indegs[v];
                if (indegs[v] == 0) {
                    q.push(v);
                }
            }
        }
        if (ans.size() != numCourses) {
            return {};
        }
        return ans;
    }
};
