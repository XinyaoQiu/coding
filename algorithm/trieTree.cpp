#include <iostream>
#include <vector>

using namespace std;

class TrieNode {
  public:
    int pass;
    int end;

    vector<TrieNode*> nexts;
    TrieNode() : pass(0), end(0) { nexts = vector<TrieNode*>(26, nullptr); }
    ~TrieNode() {
        for (TrieNode* n : nexts) {
            delete n;
        }
    }
};

class TrieTree {
  public:
    TrieNode* root;

    TrieTree() { root = new TrieNode(); }

    void insert(string word) {
        TrieNode* curr = root;
        curr->pass++;
        for (char c : word) {
            if (!curr->nexts[c - 'a']) {
                curr->nexts[c - 'a'] = new TrieNode();
            }
            curr = curr->nexts[c - 'a'];
            curr->pass++;
        }
        curr->end++;
    }

    int search(string word) {
        TrieNode* curr = root;
        for (char c : word) {
            if (!curr->nexts[c - 'a']) {
                return 0;
            }
            curr = curr->nexts[c - 'a'];
        }
        return curr->end;
    }

    int prefixNumber(string pre) {
        TrieNode* curr = root;
        for (char c : pre) {
            if (!curr->nexts[c - 'a']) {
                return 0;
            }
            curr = curr->nexts[c - 'a'];
        }
        return curr->pass;
    }

    void deleteWord(string word) {
        if (search(word)) {
            TrieNode* curr = root;
            for (char c : word) {
                if (--curr->nexts[c - 'a']->pass == 0) {
                    delete curr->nexts[c - 'a'];
                    curr->nexts[c - 'a'] = nullptr;
                    return;
                }
                curr = curr->nexts[c - 'a'];
            }
            curr->end--;
        }
    }
};