import random
from collections import deque
import copy

def your_bfs(grid_orig):
    grid = copy.deepcopy(grid_orig)
    m, n = len(grid), len(grid[0])
    if m == 1 and n == 1:
        return 0

    grid[0][0] = 2
    q = deque([(0, 0, 0, 0)])  # r, c, k_used, steps

    while q:
        r, c, k_used, steps = q.popleft()
        if r == m - 1 and c == n - 1:
            return steps

        for dr, dc in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
            nr, nc = r + dr, c + dc
            if 0 <= nr < m and 0 <= nc < n:
                val = grid[nr][nc]

                if val == 0 or val == 3:
                    grid[nr][nc] = 2
                    q.append((nr, nc, k_used, steps + 1))
                elif val == 1 and k_used == 0:
                    grid[nr][nc] = 3
                    q.append((nr, nc, 1, steps + 1))

    return -1


def ground_truth_bfs(grid):
    m, n = len(grid), len(grid[0])
    if m == 1 and n == 1:
        return 0

    max_rem_k = [[-1] * n for _ in range(m)]
    max_rem_k[0][0] = 1

    q = deque([(0, 0, 1, 0)])

    while q:
        r, c, rem_k, steps = q.popleft()
        if r == m - 1 and c == n - 1:
            return steps

        for dr, dc in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
            nr, nc = r + dr, c + dc
            if 0 <= nr < m and 0 <= nc < n:
                next_rem_k = rem_k - grid[nr][nc]
                if next_rem_k >= 0 and next_rem_k > max_rem_k[nr][nc]:
                    max_rem_k[nr][nc] = next_rem_k
                    q.append((nr, nc, next_rem_k, steps + 1))

    return -1


def generate_random_grid(rows, cols, obstacle_prob=0.3):
    grid = [[1 if random.random() < obstacle_prob else 0 for _ in range(cols)] for _ in range(rows)]
    grid[0][0] = 0
    grid[rows - 1][cols - 1] = 0
    return grid


def run_fuzz_test(num_tests=10000):
    for i in range(num_tests):
        rows = random.randint(2, 6)
        cols = random.randint(2, 6)
        grid = generate_random_grid(rows, cols, obstacle_prob=0.35)

        res_your = your_bfs(grid)
        res_gt = ground_truth_bfs(grid)

        if res_your != res_gt:
            print(f"❌ 发现卡主的反例！(第 {i+1} 次测试)")
            print("grid =")
            print(grid)
            print(f"your_bfs: {res_your}")
            print(f"ground_truth: {res_gt}")
            return grid

    print(f"✅ 完成 {num_tests} 次测试，未发现差异。")
    return None

if __name__ == "__main__":
    run_fuzz_test()