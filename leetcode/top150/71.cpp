#include <string>
#include <vector>
using namespace std;

class Solution {
  public:
    string simplifyPath(string path) {
        vector<string> dirs;
        int n = path.size();
        for (int i = 0; i < n; i++) {
            if (path[i] == '/') continue;
            string dir;
            while (i < n && path[i] != '/') {
                dir += path[i++];
            }
            if (dir == "..") {
                if (!dirs.empty()) dirs.pop_back();
            } else if (dir != "" && dir != ".") {
                dirs.push_back(dir);
            }
        }
        string ans;
        for (string dir : dirs) {
            ans += "/" + dir;
        }
        return ans == "" ? "/" : ans;
    }
};