#include <iostream>
#include <unordered_map>
#include <vector>

using namespace std;

struct Node {
    Node* next;
    int value;

    Node(int value_) : value(value_), next(nullptr) {}
};

class UnionFind {
  public:
    vector<int> fa;

    UnionFind(int n) {
        fa = vector<int>(n, -1);
    }

    int Find(int x) {
        while (fa[x] >= 0) {
            if (fa[fa[x]] >= 0) {
                fa[x] = fa[fa[x]];
            }
            x = fa[x];
        }
        return x;
    }

    bool Union(int x, int y) {
        int root_x = Find(x);
        int root_y = Find(y);
        if (root_x == root_y) {
            return false;
        }
        if (fa[root_x] > fa[root_y]) {
            fa[root_y] += fa[root_x];
            fa[root_x] = root_y;
        } else {
            fa[root_x] += fa[root_y];
            fa[root_y] = root_x;
        }
        return true;
    }

    int SetNum() {
        int count = 0;
        for (int i : fa) {
            if (i < 0) {
                count += 1;
            }
        }
        return count;
    }

    int SetCount(int x) {
        int root_x = Find(x);
        return -fa[root_x];
    }

    
};

class UnionFind2 {
    vector<int> fa;

    UnionFind2(int n) {
        for (int i = 0; i < n; i++) {
            fa.push_back(i);
        }
    }

    int Find(int x) {
        while (x != fa[x]) {
            fa[x] = fa[fa[x]];
            x = fa[x];
        }
        return x;
    }

    bool Union(int x, int y) {
        int rx = Find(x);
        int ry = Find(y);
        if (rx == ry) {
            return false;
        }
        fa[rx] = ry;
        return true;
    }

    int SetNum() {
        
    }
};

int main() {
    UnionFind uf(5);
    uf.Union(1, 3);
    cout << "uf.Count() = " << uf.SetNum() << endl;
    uf.Union(1, 2);
    uf.Union(4, 5);
    cout << "uf.Count() = " << uf.SetNum() << endl;
    cout << "uf.Find(1) = " << uf.Find(1) << endl;
    cout << "uf.Find(2) = " << uf.Find(2) << endl;
    cout << "uf.Find(3) = " << uf.Find(3) << endl;
    cout << "uf.Find(4) = " << uf.Find(4) << endl;
    cout << "uf.Find(5) = " << uf.Find(5) << endl;
    cout << "uf.SetCount(3) = " << uf.SetCount(3) << endl;
}