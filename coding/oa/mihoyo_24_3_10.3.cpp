#include <iostream>
#include <vector>
#include <queue>
#include <unordered_set>

using namespace std;

unordered_set<int> st;

int dfs(vector<vector<int>>& adj, int u, int n) {
    int cnt = 0;
    st.insert(u);
    for (int i = 1; u * i <= n; i++) {
        if (st.count(u * i)) {
            cnt++;
        }
    }
    for (int v : adj[u]) {
        if (!st.count(v)) {
            cnt += dfs(adj, v, n);
        }
    }
    st.erase(u);
    return cnt;
}

int main() {
    int n, x;
    cin >> n >> x;
    vector<vector<int>> adj(n + 1);
    for (int i = 0; i < n - 1; i++) {
        int u, v;
        cin >> u >> v;
        adj[u].push_back(v);
        adj[v].push_back(u);
    }
    cout << dfs(adj, x, n) << endl;;
}