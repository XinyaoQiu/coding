#include <string>
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
    TreeNode* helper(vector<int>& nums, int L, int R) {
        if (L > R) {
            return nullptr;
        }
        int M = L + (R - L) / 2;
        TreeNode* root = new TreeNode(nums[M]);
        root->left = helper(nums, L, M - 1);
        root->right = helper(nums, M + 1, R);
        return root;
    }
public:
    TreeNode* sortedArrayToBST(vector<int>& nums) {
        return helper(nums, 0, nums.size() - 1);
    }
};