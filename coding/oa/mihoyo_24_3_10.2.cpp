#include <iostream>
#include <vector>
#include <queue>
#include <map>

using namespace std;

int main() {
    int n, m, q;
    cin >> n >> m >> q;
    queue<string> qu;
    map<string, int> mp;
    for (int i = 0; i < q; i++) {
        int op;
        cin >> op;
        if (op == 1) {
            string name;
            cin >> name;
            if (!mp.count(name)) {
                mp[name] = 0;
            }
            qu.push(name);
        } else if (op == 2) {
            string name;
            cin >> name;
            while (qu.front() != name) {
                qu.pop();
            }
            qu.pop();
        } else if (op == 3) {
            int x;
            cin >> x;
            mp[qu.front()] += x;
            n -= x;
        } else if (op == 4) {
            cout << qu.size() << endl;
        }
        if (n <= m) {
            mp[qu.front()] += m;
            break;
        }
    }
    for (auto& [k, v] : mp) {
        cout << k << " " << v << endl;;
    }
}