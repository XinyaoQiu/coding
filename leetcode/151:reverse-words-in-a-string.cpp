#include "leetcode_base.h"

class Solution {
public:
    string reverseWords(string s) {
        string word = "";
        vector<string> words;
        for (char c : s) {
            if (c == ' ') {
                if (word != "") {
                    words.push_back(word);
                    word = "";
                }
            } else {
                word += c;
            }
        }
        if (word != "") {
            words.push_back(word);
        }
        string result;
        while (!words.empty()) {
            string word = words.back();
            words.pop_back();
            result += word;
            result += " ";
        }
        result.pop_back();
        return result;
    }
};