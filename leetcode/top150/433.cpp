#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <utility>
using namespace std;

class Solution {
    const string CHOICES[4] = {"A", "C", "T", "G"};
public:
    int minMutation(string startGene, string endGene, vector<string>& bank) {
        unordered_set<string> s(bank.begin(), bank.end());
        queue<pair<string, int>> q;
        unordered_map<string, int> visited;
        q.emplace(startGene, 0);
        visited[startGene] = 1;
        while (!q.empty()) {
            auto [gene, step] = q.front();
            q.pop();
            for (int i = 0; i < 8; ++i) {
                for (int j = 0; j < 4; ++j) {
                    string new_gene = gene;
                    new_gene.replace(i, 1, CHOICES[j]);
                    if (!visited[new_gene]) {
                        visited[new_gene] = 1;
                        if (!s.count(new_gene)) {
                            continue;
                        }
                        if (new_gene == endGene) {
                            return step + 1;
                        }
                        q.emplace(new_gene, step + 1);
                    }
                }
            }
        }
        return -1;
    }
};

