#include <string>
#include <vector>
using namespace std;

class Solution {
public:
    string simplifyPath(string path) {
        vector<string> names;
        string name;
        for (char c : path) {
            if (c == '/') {
                if (name == "..") {
                    if (!names.empty()) {
                        names.pop_back();
                    }
                } else if (name != "" && name != ".") {
                    names.emplace_back(name);
                }
                name.clear();
            } else {
                name += c;
            }
        }
        if (name == "..") {
            if (!names.empty()) {
                names.pop_back();
            }
        } else if (name != "" && name != ".") {
            names.emplace_back(name);
        }
        string ans;
        for (string& name : names) {
            ans += "/" + name;
        }
        return ans == "" ? "/": ans;
    }
};