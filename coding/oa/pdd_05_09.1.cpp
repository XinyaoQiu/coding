#include <iostream>
#include <vector>

using namespace std;

void process(int money, int fri) {
    vector<vector<int>> arr;
    for (int i = 0; i <= money; i++) {
        vector<int> temp(fri + 1, 0);
        arr.push_back(temp);
    }
    if (money < 5) {
        cout << 0 << " " << 0 << endl;
    }
    for (int i = 5; i <= money; i++) {
        for (int j = 0; j <= fri; j++) {
            for (int k = 0; k < 6; k++) {
                arr[i][j] = max(arr[i][j], i >= 10 - k && j >= k ? arr[i - 10 + k][j - k] + 1 : 0) ;
            }
        }
    }
    cout << arr[money][fri] << " ";
    for (int i = 0; i <= money; i++) {
        for (int j = 0; j <= fri; j++) {
            if (arr[i][j] == arr[money][fri]) {
                cout << i << endl;
                return;
            }
        }
    }
}

int main() {
    int n;
    cin >> n;
    for (int i = 0; i < n; i++) {
        int money, fri;
        cin >> money >> fri;
        process(money, fri);
    }
}