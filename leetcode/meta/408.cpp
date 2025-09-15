#include <vector>
#include <string>
#include <sstream>
#include <numeric>
using namespace std;

class Solution {
public:
    bool validWordAbbreviation(string word, string abbr) {
        int i = 0, j = 0;
        while (i < word.size() && j < abbr.size()) {
            if (islower(abbr[j])) {
                if (word[i] != abbr[j]) {
                    return false;
                }
                ++i;
                ++j;
            } else if (isdigit(abbr[j])) {
                if (abbr[j] == '0') {
                    return false;
                }
                string s_num;
                while (j < abbr.size() && isdigit(abbr[j])) {
                    s_num.push_back(abbr[j]);
                    ++j;
                }
                istringstream iss(s_num);
                int num;
                iss >> num;
                i += num;
            }
        }
        return i == word.size() && j == abbr.size();
    }
};