#include <string>
#include <vector>
#include <queue>
using namespace std;

class WordDictionary {
    struct TreeNode {
        int end;
        vector<TreeNode*> children;
        TreeNode() : end(0), children(26) {}
    };
    TreeNode* root;
public:
    WordDictionary() {
        root = new TreeNode();
    }
    
    void addWord(string word) {
        TreeNode* cur = root;
        for (char it : word) {
            if (!cur->children[it - 'a']) {
                cur->children[it - 'a'] = new TreeNode();
            }
            cur = cur->children[it - 'a'];
        }
        cur->end++;
    }

    bool dfs(TreeNode* root, string& word, int i) {
        if (!root) {
            return false;
        }
        if (i == word.size()) {
            return root->end > 0;
        }
        if (word[i] == '.') {
            for (TreeNode* child : root->children) {
                if (dfs(child, word, i + 1)) {
                    return true;
                }
            }
            return false;
        } else {
            return dfs(root->children[word[i] - 'a'], word, i + 1);
        }
    }
    
    bool search(string word) {
        return dfs(root, word, 0);
    }
};

/**
 * Your WordDictionary object will be instantiated and called as such:
 * WordDictionary* obj = new WordDictionary();
 * obj->addWord(word);
 * bool param_2 = obj->search(word);
 */