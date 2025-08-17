#include <iostream>
#include <string>
#include <vector>
using namespace std;

void push_line(vector<string>& ans, string& line, int width) {
    int remain = width - line.size();
    line = string(remain / 2, ' ') + line + string(remain / 2, ' ') +
           (remain % 2 ? " " : "");
    ans.push_back("*" + line + "*");
}

vector<string> solution(vector<vector<string>>& paragraphs, int width) {
    vector<string> ans({string(width + 2, '*')});
    for (auto& paragraph : paragraphs) {
        string line;
        for (auto& s : paragraph) {
            if (line.size() + s.size() + 1 > width) {
                push_line(ans, line, width);
                line = "";
            }
            if (line != "") {
                line = line + " " + s;
            } else {
                line = s;
            }
        }
        if (line != "") {
            push_line(ans, line, width);
        }
    }
    ans.push_back(string(width + 2, '*'));
    return ans;
}

int main() {
    vector<vector<string>> paragraphs;
    paragraphs.push_back({"hello", "world"});
    paragraphs.push_back({"How", "areYou", "doing"});
    paragraphs.push_back({"Please", "look", "and", "align", "to", "the", "center"});
    vector<string> ans = solution(paragraphs, 16);
    for (auto& s : ans) {
        cout << s << endl;
    }
}