#include <string>
#include <vector>
using namespace std;

class Trie {
    struct TreeNode {
        int end;
        vector<TreeNode*> children;
        TreeNode() : end(0), children(26) {}
    };
    TreeNode* root;
public:
    Trie() {
        root = new TreeNode();
    }
    
    void insert(string word) {
        TreeNode* cur = root;
        for (char it : word) {
            if (!cur->children[it - 'a']) {
                cur->children[it - 'a'] = new TreeNode();
            }
            cur = cur->children[it - 'a'];
        }
        cur->end++;
    }
    
    bool search(string word) {
        TreeNode* cur = root;
        for (char it : word) {
            if (!cur->children[it - 'a']) {
                return false;
            }
            cur = cur->children[it - 'a'];
        }
        return cur->end > 0;
    }
    
    bool startsWith(string prefix) {
        TreeNode* cur = root;
        for (char it : prefix) {
            if (!cur->children[it - 'a']) {
                return false;
            }
            cur = cur->children[it - 'a'];
        }
        return true;
    }
};


/**
 * Your Trie object will be instantiated and called as such:
 * Trie* obj = new Trie();
 * obj->insert(word);
 * bool param_2 = obj->search(word);
 * bool param_3 = obj->startsWith(prefix);
 */