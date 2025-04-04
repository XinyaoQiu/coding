#include <iostream>
#include <vector>
#include <unordered_set>

using namespace std;

int main() {
    int n;
    cin >> n;
    vector<int> map(n + 2);
    vector<int> dirs(n + 1);
    for (int i = 1; i <= n; i++) {
        map[i] = i;
        cin >> dirs[i];
    }
    int count = 0;
    while (count++ < n) {
        unordered_set<int> set;
        for (int i = 1; i <= n; i++) {
            if (dirs[i] == 0) {
                map[i] = max(0, map[i] - 1);
            } else {
                map[i] = min(n + 1, map[i] + 1);
            }
            if (map[i] >= 1 && map[i] <= n) {
                set.insert(i);
            }
        }
        cout << n - set.size() << " ";
    }
    cout << endl;
}