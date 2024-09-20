#include <iostream>
#include <vector>
#include <tuple>
#include <sstream>

using namespace std;

pair<int, int> partition(vector<int>& arr, int L, int R) {
    int i = L, p = R--;
    while (i <= R) {
        if (arr[i] < arr[p]) {
            swap(arr[i++], arr[L++]);
        } else if (arr[i] > arr[p]) {
            swap(arr[i], arr[R--]);
        } else {
            i++;
        }
    }
    return { L - 1, R + 1 };
}

void qsort(vector<int>& arr, int L, int R) {
    if (L >= R) return;
    int p = L + rand() % (R - L);
    swap(arr[p], arr[R]);
    int left, right;
    tie(left, right) = partition(arr, L, R);
    qsort(arr, L, left);
    qsort(arr, right, R);
}

int main() {
    vector<int> arr;
    string line;
    getline(cin, line);
    istringstream iss(line);
    int num;
    while (iss >> num) {
        arr.push_back(num);
    }
    qsort(arr, 0, arr.size() - 1);
    for (int i = 0; i < arr.size(); i++) {
        cout << arr[i] << " ";
    }
    cout << endl;
}