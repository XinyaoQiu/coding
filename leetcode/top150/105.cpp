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
    TreeNode* build(vector<int>& preorder, vector<int>& inorder, int pL, int pR, int iL, int iR) {
        if (pL > pR) {
            return nullptr;
        }
        int root_val = preorder[pL];
        int index = 0;
        for (int i = iL; i <= iR; ++i) {
            if (inorder[i] == root_val) {
                index = i;
                break;
            }
        }
        TreeNode* left = build(preorder, inorder, pL + 1, pL + index - iL, iL, index - 1);
        TreeNode* right = build(preorder, inorder, pL + index - iL + 1, pR, index + 1, iR);
        return new TreeNode(root_val, left, right);
    }
  public:
    TreeNode* buildTree(vector<int>& preorder, vector<int>& inorder) {
        int pL = 0, pR = preorder.size() - 1, iL = 0, iR = inorder.size() - 1;
        return build(preorder, inorder, pL, pR, iL, iR);
    }
};