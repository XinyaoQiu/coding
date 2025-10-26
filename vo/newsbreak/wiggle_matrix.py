matrix = [
    [5, 1, 3],
    [4, 2, 6],
    [3, 5, 8],
    [7, 2, 4],
]

def func(matrix):
    m, n = len(matrix), len(matrix[0])
    def get_ij(x):
        return x // n, x % n
    for x in range(m * n - 1):
        i, j = get_ij(x)
        ni, nj = get_ij(x + 1)
        if (x % 2 == 0 and matrix[i][j] > matrix[ni][nj]) or (x % 2 == 1 and matrix[i][j] < matrix[ni][nj]):
            matrix[i][j], matrix[ni][nj] = matrix[ni][nj], matrix[i][j]

func(matrix)
print(matrix)