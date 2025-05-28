#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

class Solution {
  public:
    bool wordPattern(string pattern, string s) {
        unordered_map<char, string> p_to_s;
        unordered_map<string, char> s_to_p;
        vector<string> words;
        string word;
        for (int i = 0; i < s.size(); ++i) {
            if (s[i] != ' ') {
                word += s[i];
            } else {
                words.emplace_back(word);
                word = "";
            }
        }
        words.emplace_back(word);
        if (pattern.size() != words.size()) {
            return false;
        }
        for (int i = 0; i < pattern.size(); ++i) {
            if (p_to_s.count(pattern[i]) && p_to_s[pattern[i]] != words[i]) {
                return false;
            }
            if (s_to_p.count(words[i]) && s_to_p[words[i]] != pattern[i]) {
                return false;
            }
            p_to_s[pattern[i]] = words[i];
            s_to_p[words[i]] = pattern[i];
        }
        return true;
    }
};