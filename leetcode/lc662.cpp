#include <iostream>
#include <queue>
#include <unordered_map>

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
    int widthOfBinaryTree(TreeNode* root) {
        queue<TreeNode*> q;
        unordered_map<TreeNode*, unsigned long long> levels;
        unordered_map<TreeNode*, unsigned long long> indexes;
        q.push(root);
        levels[root] = 0;
        indexes[root] = 1;
        unsigned long long maximum = 0;
        unsigned long long curLevel = 0;
        unsigned long long first = 1;
        while (!q.empty()) {
            TreeNode* curr = q.front();
            if (levels[curr] != curLevel) {
                curLevel++;
                first = indexes[curr];
            }
            maximum = max(maximum, indexes[curr] - first + 1);
            q.pop();
            if (curr->left) {
                levels[curr->left] = curLevel + 1;
                indexes[curr->left] = indexes[curr] * 2;
                q.push(curr->left);
            }
            if (curr->right) {
                levels[curr->right] = curLevel + 1;
                indexes[curr->right] = indexes[curr] * 2 + 1;
                q.push(curr->right);
            }
        }
        return maximum;
    }
};