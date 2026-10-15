#include <vector>
#include <queue>
using namespace std;

class Solution {
    vector<vector<int>> edges;
    vector<int> indegs;
public:
    bool canFinish(int numCourses, vector<vector<int>>& prerequisites) {
        edges.resize(numCourses);
        indegs.resize(numCourses);
        for (auto& v : prerequisites) {
            edges[v[1]].push_back(v[0]);
            ++indegs[v[0]];
        }
        queue<int> q;
        vector<int> ans;
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            ans.push_back(u);
            for (int v : edges[u]) {
                if (--indegs[v] == 0) {
                    q.push(v);
                }
            }
        }
        return ans.size() == numCourses;
    }
};