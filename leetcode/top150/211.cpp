#include <string>
#include <vector>
using namespace std;

/*
             0
        /'a' |'b' \'c'
       0     1     1
      /'d'          \'e'
     1               1
*/

class WordDictionary {
    struct TrieNode {
        bool isEnd;
        vector<TrieNode*> nexts;
        TrieNode() : isEnd(false), nexts(26, nullptr) {}
    };
    TrieNode* root;
  public:
    WordDictionary() : root(new TrieNode()) {}

    void addWord(string word) {
        TrieNode* cur = root;
        for (char c : word) {
            if (!cur->nexts[c - 'a']) {
                cur->nexts[c - 'a'] = new TrieNode();
            }
            cur = cur->nexts[c - 'a'];
        }
        cur->isEnd = true;
    }

    bool dfs(string& word, int i, TrieNode* root) {
        if (i == word.size()) {
            return root->isEnd;
        }
        if (word[i] == '.') {
            for (auto next : root->nexts) {
                if (next && dfs(word, i + 1, next)) {
                    return true;
                }
            }
        } else {
            TrieNode* next = root->nexts[word[i] - 'a'];
            if (next) {
                return dfs(word, i + 1, next);
            }
        }
        return false;
    }

    bool search(string word) {
        return dfs(word, 0, root);
    }
    
};

/**
 * Your WordDictionary object will be instantiated and called as such:
 * WordDictionary* obj = new WordDictionary();
 * obj->addWord(word);
 * bool param_2 = obj->search(word);
 */