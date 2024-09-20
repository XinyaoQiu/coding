#include <iostream>
#include <vector>

using namespace std;

vector<int> generateNext(string p) {
    vector<int> nexts(p.length(), 0);
    int m = p.length();
    int i = 0;
    for (int j = 1; j < m; j++) {
        while (i > 0 && p[i] != p[j]) {
            i = nexts[i - 1];
        }
        if (p[i] == p[j]) {
            i++;
        }
        nexts[j] = i;
    }
    return nexts;
}

int kmp(string t, string p) {
    int n = t.length(), m = p.length();
    vector<int> nexts = generateNext(p);
    int i = 0;
    for (int j = 0; j < n; j++) {
        while (i > 0 && p[i] != t[j]) {
            i = nexts[i - 1];
        }
        if (p[i] == t[j]) {
            i++;
        }
        if (i == m) {
            return j - m + 1;
        }
    }
    return -1;
}

int main() {
    cout << kmp("absefoseicjie", "sei") << endl;
}