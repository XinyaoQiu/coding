#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

int merge(vector<int> &arr, int L, int M, int R) {
    vector<int> helper(R - L + 1);
    int p1 = L, p2 = M + 1, i = 0, res = 0;
    while (p1 <= M && p2 <= R) {
        res += arr[p1] > arr[p2] ? (R - p2 + 1) : 0;
        helper[i++] = arr[p1] > arr[p2] ? arr[p1++] : arr[p2++];
    }
    while (p1 <= M) {
        helper[i++] = arr[p1++];
    }
    while (p2 <= R) {
        helper[i++] = arr[p2++];
    }
    for (int &num : helper) {
        arr[L++] = num;
    }
    return res;
}


int ni_xu_dui_helper(vector<int> &arr, int L, int R) {
    if (L == R) {
        return 0;
    }
    int M = L + ((R - L) >> 1);
    return ni_xu_dui_helper(arr, L, M) + ni_xu_dui_helper(arr, M + 1, R) + merge(arr, L, M, R);
}

int ni_xu_dui(vector<int> arr) {
    if (arr.size() < 2) {
        return 0;
    }
    return ni_xu_dui_helper(arr, 0, arr.size() - 1);
}

int main() {
    string line;
    getline(cin, line);
    istringstream iss(line);
    vector<int> arr;
    int num;
    while (iss >> num) {
        arr.push_back(num);
    }
    cout << ni_xu_dui(arr) << endl;
}