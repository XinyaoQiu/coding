#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

int merge(vector<int> &arr, int L, int M, int R) {
    vector<int> helper(R - L + 1);
    int i = 0, p1 = L, p2 = M + 1;
    int res = 0;
    while (p1 <= M && p2 <= R) {
        res += arr[p1] < arr[p2] ? (R - p2 + 1) * arr[p1] : 0;
        helper[i++] = arr[p1] < arr[p2] ? arr[p1++] : arr[p2++];
    }
    while (p1 <= M) {
        helper[i++] = arr[p1++];
    }
    while (p2 <= R) {
        helper[i++] = arr[p2++];
    }
    return res;
}

int littlesum_helper(vector<int> &arr, int L, int R) {
    if (L == R) {
        return 0;
    }
    int M = L + ((R - L) >> 1);
    return littlesum_helper(arr, L, M) + littlesum_helper(arr, M + 1, R) +
           merge(arr, L, M, R);
}

int littlesum(vector<int> &arr) {
    if (arr.size() < 2) {
        return 0;
    }
    return littlesum_helper(arr, 0, arr.size() - 1);
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
    cout << littlesum(arr) << endl;
}