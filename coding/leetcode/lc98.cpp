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
    struct ReturnData {
        bool isBST;
        int min;
        int max;
        ReturnData(bool a, int b, int c) : isBST(a), min(b), max(c) {}
    };

    ReturnData process(TreeNode* root) {
        bool isBST = true;
        int minVal = root->val;
        int maxVal = root->val;
        if (root->left) {
            ReturnData left = process(root->left);
            isBST = isBST && left.isBST && left.max < root->val;
            minVal = min(minVal, left.min);
            maxVal = max(maxVal, left.max);
        }
        if (root->right) {
            ReturnData right = process(root->right);
            isBST = isBST && right.isBST && root->val < right.min;
            minVal = min(minVal, right.min);
            maxVal = max(maxVal, right.max);
        }
        return ReturnData(isBST, minVal, maxVal);
    }

    bool isValidBST(TreeNode* root) { return process(root).isBST; }
};