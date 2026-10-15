#include <iostream>
#include <vector>
#include <cmath>

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

struct Info {
    bool isFull;
    int height;
    int nodesNum;
    Info(bool _isFull, int _height, int _nodesNum)
        : isFull(_isFull), height(_height), nodesNum(_nodesNum) {}
};

Info process(TreeNode* root) {
    if (!root) {
        return Info(true, 0, 0);
    }
    Info leftData = process(root->left);
    Info rightData = process(root->right);
    int height = max(leftData.height, rightData.height) + 1;
    int nodesNum = leftData.nodesNum + rightData.nodesNum + 1;
    bool isFull = nodesNum == (1 << height) - 1;
}

bool isFullBinaryTree(TreeNode* root) {
    return process(root).isFull;
}