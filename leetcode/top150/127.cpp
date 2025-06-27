#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <queue>
using namespace std;

class Solution {
public:
    int ladderLength(string beginWord, string endWord, vector<string>& wordList) {
        unordered_set<string> wordSet(wordList.begin(), wordList.end());
        unordered_map<string, int> visited;
        queue<pair<string, int>> q;
        int len = beginWord.size();
        q.emplace(beginWord, 1);
        while (!q.empty()) {
            auto [word, step] = q.front();
            q.pop();
            for (int i = 0; i < len; ++i) {
                for (int j = 0; j < 26; ++j) {
                    string newWord = word;
                    newWord.replace(i, 1, string(1, 'a' + j));
                    if (!wordSet.count(newWord)) {
                        continue;
                    }
                    if (newWord == endWord) {
                        return step + 1;
                    }
                    if (!visited[newWord]) {
                        visited[newWord] = 1;
                        q.emplace(newWord, step + 1);
                    }
                }
            }
        }
        return 0;
    }
};