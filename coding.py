# lc348 Tic-Tac-Toe
class TicTacToe:
    def __init__(self, n):
        self.n = n
        self.rows = [0] * n
        self.cols = [0] * n
        self.diag = 0
        self.anti = 0

    def move(self, row, col, player):
        delta = 1 if player == 1 else -1
        self.rows[row] += delta
        self.cols[col] += delta
        if row == col:
            self.diag += delta
        if row + col == self.n - 1:
            self.anti += delta

        if self.n in (abs(self.rows[row]), abs(self.cols[col]), abs(self.diag), abs(self.anti)):
            return player

        return 0

