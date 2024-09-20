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
    struct ReturnType {
        bool isBalanced;
        int height;
        ReturnType(bool x, int y) : isBalanced(x), height(y) {}
    };

    ReturnType process(TreeNode* node) {
        if (!node) {
            return ReturnType(true, 0);
        }
        ReturnType left = process(node->left);
        ReturnType right = process(node->right);
        bool isBalanced = left.isBalanced && right.isBalanced &&
                          abs(left.height - right.height) <= 1;
        int height = max(left.height, right.height) + 1;
        return ReturnType(isBalanced, height);
    }

    bool isBalanced(TreeNode* root) {
        return process(root).isBalanced;
    }
};