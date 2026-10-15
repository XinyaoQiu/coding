#include <iostream>
#include <vector>

using namespace std;

struct TreeNode {
    int value;
    TreeNode* left;
    TreeNode* right;
    TreeNode() : value(0), left(nullptr), right(nullptr) {}
    TreeNode(int value_) : value(value_), left(nullptr), right(nullptr) {}
};

void preOrderNonRecur(TreeNode* root) {
    vector<TreeNode*> stack;
    stack.push_back(root);
    while (!stack.empty()) {
        TreeNode* curr = stack.back();
        stack.pop_back();
        cout << curr->value << " ";
        if (curr->right)
            stack.push_back(curr->right);
        if (curr->left)
            stack.push_back(curr->left);
    }
    cout << endl;
}

void posOrderNonRecur(TreeNode* root) {
    vector<TreeNode*> stack;
    vector<TreeNode*> container;
    stack.push_back(root);
    while (!stack.empty()) {
        TreeNode* curr = stack.back();
        stack.pop_back();
        container.push_back(curr);
        if (curr->left)
            stack.push_back(curr->left);
        if (curr->right)
            stack.push_back(curr->right);
    }
    while (!container.empty()) {
        cout << container.back()->value << " ";
        container.pop_back();
    }
    cout << endl;
}

void inOrderNonRecur(TreeNode* root) {
    vector<TreeNode*> stack;
    TreeNode* curr = root;
    while (curr) {
        stack.push_back(curr);
        curr = curr->left;
    }
    while (!stack.empty()) {
        curr = stack.back();
        stack.pop_back();
        cout << curr->value << " ";
        curr = curr->right;
        while (curr) {
            stack.push_back(curr);
            curr = curr->left;
        }
    }
    cout << endl;
}

int main() {}