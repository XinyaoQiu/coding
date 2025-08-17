#include <iostream>
#include <vector>
#include <unordered_set>
using namespace std;

int solution(string text) {
    unordered_set<char> s({'a', 'e', 'i', 'o', 'u'});
    if (text.size() < 3) {
        return 0;
    }
    int curr = 0, ans = 0;
    for (int i = 0; i < 3; ++i) {
        if (s.count(text[i])) {
            ++curr;
        }
    }
    if (curr == 2) {
        ++ans;
    }
    for (int i = 3; i < text.size(); ++i) {
        if (s.count(text[i - 3])) {
            --curr;
        }
        if (s.count(text[i])) {
            ++curr;
        }
        if (curr == 2) {
            ++ans;
        }
    }
    return ans;
}

int main() {
    string text = "aeiobe";
    cout << solution(text) << endl;
}