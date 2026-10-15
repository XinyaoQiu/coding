#include <vector>
#include <unordered_map>
#include <algorithm>
#include <queue>
using namespace std;

struct TreeNode {
    int val;
    TreeNode* left;
    TreeNode* right;
    TreeNode() : val(0), left(nullptr), right(nullptr) {}
    TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
    TreeNode(int x, TreeNode* left, TreeNode* right)
        : val(x), left(left), right(right) {}
};

class Solution {
  public:
    vector<vector<int>> verticalOrder(TreeNode* root) {
        if (!root) {
            return {};
        }
        unordered_map<int, vector<int>> mp;
        queue<pair<TreeNode*, int>> q;
        q.push({root, 0});
        while (!q.empty()) {
            auto p = q.front();
            q.pop();
            mp[p.second].push_back(p.first->val);
            if (p.first->left) {
                q.push({p.first->left, p.second - 1});
            }
            if (p.first->right) {
                q.push({p.first->right, p.second + 1});
            }
        }
        vector<vector<int>> res;
        vector<pair<int, vector<int>>> v(mp.begin(), mp.end());
        sort(v.begin(), v.end(), [](const pair<int, vector<int>>& a, const pair<int, vector<int>>& b) {
            return a.first < b.first;
        });
        for (auto& p : v) {
            res.push_back(p.second);
        }
        return res;
    }
};