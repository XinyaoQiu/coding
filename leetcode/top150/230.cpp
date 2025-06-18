#include <iostream>
#include <vector>
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
    int kthSmallest(TreeNode* root, int k) {
        vector<TreeNode*> stack;
        while (root || !stack.empty()) {
            while (root) {
                stack.push_back(root);
                root = root->left;
            }
            root = stack.back();
            stack.pop_back();
            --k;
            if (k == 0) {
                break;
            }
            root = root->right;
        }
        return root->val;
    }
};