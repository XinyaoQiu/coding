#include <iostream>
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
// v = 5, h = 3
// bits = 0b100
// 5 = 0b101

class Solution {
    bool exists(TreeNode* root, int v, int h) {
        int bits = 1 << h - 1;
        while (root && bits) {
            if (v & bits) {
                root = root->right;
            } else {
                root = root->left;
            }
            bits >>= 1;
        }
        return root;
    }
public:
    int countNodes(TreeNode* root) {
        if (!root) {
            return 0;
        }
        TreeNode* node = root;
        int h = 0;
        while (node->left) {
            node = node->left;
            ++h;
        }
        int L = (1 << h) + 1, R = (1 << h + 1) - 1;
        while (L <= R) {
            int M = L + (R - L) / 2;
            if (exists(root, M, h)) {
                L = M + 1;
            } else {
                R = M - 1;
            }
        }
        return R;
    }
};
