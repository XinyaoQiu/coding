#include <vector>
#include <string>
using namespace std;

class Solution {
    string path;
    vector<string> ans;
    void dfs(int n, int leftCount, int rightCount) {
        if (path.size() == 2 * n) {
            if (leftCount == rightCount) {
                ans.emplace_back(path);
            }
            return;
        }
        if (leftCount >= rightCount && leftCount < n) {
            path.push_back('(');
            dfs(n, leftCount + 1, rightCount);
            path.pop_back();
        }
        if (leftCount > rightCount && rightCount < n) {
            path.push_back(')');
            dfs(n, leftCount, rightCount + 1);
            path.pop_back();
        }
    }
public:
    vector<string> generateParenthesis(int n) {
        dfs(n, 0, 0);
        return ans;
    }
};