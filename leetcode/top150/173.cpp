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

class BSTIterator {
    vector<TreeNode*> stack;
    TreeNode* root;
public:
    BSTIterator(TreeNode* root) : root(root) {}
    
    int next() {
        while (root) {
            stack.push_back(root);
            root = root->left;
        }
        root = stack.back();
        stack.pop_back();
        int ans = root->val;
        root = root->right;
        return ans;
    }
    
    bool hasNext() {
        return root || !stack.empty();
    }
};