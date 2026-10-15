#include <iostream>
#include <deque>

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
    bool isCompleteTree(TreeNode* root) {
        deque<TreeNode*> dq;
        dq.push_back(root);
        bool isLeaf = false;
        while (!dq.empty()) {
            TreeNode* node = dq.front();
            dq.pop_front();
            if ((!node->left && node->right) || (isLeaf && node->left)) {
                return false;
            }   
            if (node->left) {
                dq.push_back(node->left);
            }
            if (node->right) {
                dq.push_back(node->right);
            }
            if (!node->left || !node->right) {
                isLeaf = true;
            }
        }
        return true;
    }

};