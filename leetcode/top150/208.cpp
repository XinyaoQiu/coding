#include <string>
#include <vector>
using namespace std;

class Trie {
    struct TrieNode {
        int end;
        vector<TrieNode*> nexts;
        TrieNode() : end(0), nexts(26, nullptr) {}
    };
    TrieNode* root;
  public:
    Trie() : root(new TrieNode()) {}

    void insert(string word) {
        TrieNode* cur = root;
        for (char c : word) {
            if (!cur->nexts[c - 'a']) {
                cur->nexts[c - 'a'] = new TrieNode();
            }
            cur = cur->nexts[c - 'a'];
        }
        cur->end++;
    }

    bool search(string word) {
        TrieNode* cur = root;
        for (char c : word) {
            if (!cur->nexts[c - 'a']) {
                return false;
            }
            cur = cur->nexts[c - 'a'];
        }
        return cur->end > 0;
    }

    bool startsWith(string prefix) {
        TrieNode* cur = root;
        for (char c : prefix) {
            if (!cur->nexts[c - 'a']) {
                return false;
            }
            cur = cur->nexts[c - 'a'];
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