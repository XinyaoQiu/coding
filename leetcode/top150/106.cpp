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
    TreeNode* build(vector<int>& postorder, vector<int>& inorder, int pL, int pR, int iL, int iR) {
        if (pL > pR) {
            return nullptr;
        }
        int val = postorder[pR];
        int iM = 0;
        for (int i = iL; i <= iR; ++i) {
            if (inorder[i] == val) {
                iM = i;
                break;
            }
        }
        TreeNode* left = build(postorder, inorder, pL, pL + iM - iL - 1, iL, iM - 1);
        TreeNode* right = build(postorder, inorder, pL + iM - iL, pR - 1, iM + 1, iR);
        return new TreeNode(val, left, right);
    }
  public:
    TreeNode* buildTree(vector<int>& inorder, vector<int>& postorder) {
        return build(postorder, inorder, 0, postorder.size() - 1, 0, inorder.size() - 1);
    }
};