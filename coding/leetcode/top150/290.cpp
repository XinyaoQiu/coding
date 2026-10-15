#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
using namespace std;

class Solution {
  public:
    bool wordPattern(string pattern, string s) {
        istringstream iss(s);
        vector<string> words;
        string word;
        while (iss >> word) {
            words.emplace_back(word);
        }
        if (pattern.size() != words.size()) {
            return false;
        }
        unordered_map<char, string> ctos;
        unordered_map<string, char> stoc;
        for (int i = 0; i < pattern.size(); ++i) {
            if (ctos.count(pattern[i]) && ctos[pattern[i]] != words[i]) {
                return false;
            }
            if (stoc.count(words[i]) && stoc[words[i]] != pattern[i]) {
                return false;
            }
            ctos[pattern[i]] = words[i];
            stoc[words[i]] = pattern[i];
        }
        return true;
    }
};