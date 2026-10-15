#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <queue>
#include <climits>
using namespace std;

class Solution {
public:
    int ladderLength(string beginWord, string endWord, vector<string>& wordList) {
        unordered_set<string> wordSet(wordList.begin(), wordList.end());
        if (!wordSet.count(endWord)) {
            return 0;
        }
        queue<string> q({beginWord});
        unordered_map<string, int> step;
        step[beginWord] = 1;
        while (!q.empty()) {
            string word = q.front();
            q.pop();
            int currStep = step[word];
            for (char& it : word) {
                char tmp = it;
                for (int i = 'a'; i <= 'z'; ++i) {
                    it = i;
                    if (!wordSet.count(word)) {
                        continue;
                    }
                    if (word == endWord) {
                        return currStep + 1;
                    }
                    if (step[word] == 0) {
                        step[word] = currStep + 1;
                        q.emplace(word);
                    }
                }
                it = tmp;
            }
        }
        return 0;
    }
};