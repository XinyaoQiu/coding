#include <iostream>
#include <sstream>
#include <vector>
using namespace std;

void merge(vector<int> &arr, int L, int M, int R) {
    vector<int> helper(R - L + 1, 0);
    int i = 0, p1 = L, p2 = M + 1;
    while (p1 <= M && p2 <= R) {
        helper[i++] = arr[p1] < arr[p2] ? arr[p1++] : arr[p2++];
    }
    while (p1 <= M) {
        helper[i++] = arr[p1++];
    }
    while (p2 <= R) {
        helper[i++] = arr[p2++];
    }
    for (size_t i = 0; i < helper.size(); i++) {
        arr[L++] = helper[i];
    }
}

void process(vector<int> &arr, int L, int R) {
    if (L == R)
        return;
    int M = L + ((R - L) >> 1);
    process(arr, L, M);
    process(arr, M + 1, R);
    merge(arr, L, M, R);
}

void mergesort(vector<int> &arr) {
    if (arr.size() < 2) {
        return;
    }
    process(arr, 0, arr.size() - 1);
}

int main() {
    vector<int> arr;
    string inputline;
	getline(cin, inputline);
	istringstream iss(inputline);
	int num;
	while (iss >> num) {
		arr.push_back(num);
	}
	mergesort(arr);
    for (int &i : arr) {
        cout << i << " ";
    }
    cout << endl;
}