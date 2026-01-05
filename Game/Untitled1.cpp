#include <stdio.h>

int x[9][9];      // Ma tr?n Sudoku
int dem = 0;      // Ð?m s? l?i gi?i

// Hàm ki?m tra giá tr? v có th? d?t t?i ô (r, c) hay không
int check(int r, int c, int v) {
    // Ki?m tra trên dòng r
    for (int j = 0; j < 9; j++)
        if (x[r][j] == v) return 0;

    // Ki?m tra trên c?t c
    for (int i = 0; i < 9; i++)
        if (x[i][c] == v) return 0;

    // Ki?m tra trong kh?i 3x3
    int rStart = (r / 3) * 3;
    int cStart = (c / 3) * 3;
    for (int i = rStart; i < rStart + 3; i++)
        for (int j = cStart; j < cStart + 3; j++)
            if (x[i][j] == v) return 0;

    return 1;
}

// Hàm in ra l?i gi?i hi?n t?i
void solution() {
    printf("Cách di?n th? %d:\n", dem);
    for (int i = 0; i < 9; i++) {
        if (i == 3 || i == 6) printf("---------------------\n");
        for (int j = 0; j < 9; j++) {
            printf("%d ", x[i][j]);
            if (j == 2 || j == 5) printf("| ");
        }
        printf("\n");
    }
    printf("\n");
}

// Hàm quay lui d? tìm l?i gi?i
void Try(int r, int c) {
    if (c == 9) {             // sang c?t ngoài b?ng
        if (r == 8) {         // dã di?n xong hàng cu?i
            dem++;
            solution();
            return;
        } else {
            r++;
            c = 0;
        }
    }

    if (x[r][c] != 0) {
        // Ô dã có s?n, chuy?n sang ô k? ti?p
        Try(r, c + 1);
    } else {
        // Th? các giá tr? t? 1 d?n 9
        for (int v = 1; v <= 9; v++) {
            if (check(r, c, v)) {
                x[r][c] = v;
                Try(r, c + 1);
                x[r][c] = 0;  // backtrack
            }
        }
    }
}

int main() {
    // Kh?i t?o d? Sudoku
    int init[9][9] = {
        {3,0,6,5,0,8,4,0,0},
        {5,2,0,0,0,0,0,0,0},
        {0,8,7,0,0,0,0,3,1},
        {0,0,3,0,1,0,0,8,0},
        {9,0,0,8,6,3,0,0,5},
        {0,5,0,0,9,0,6,0,0},
        {1,3,0,0,0,0,2,5,0},
        {0,0,0,0,0,0,0,7,4},
        {0,0,5,2,0,6,3,0,0}
    };

    // Sao chép vào ma tr?n x
    for (int i = 0; i < 9; i++)
        for (int j = 0; j < 9; j++)
            x[i][j] = init[i][j];

    printf("Bài Sudoku ban d?u:\n");
    solution();   // In d? ban d?u
    printf("B?t d?u tìm l?i gi?i...\n\n");

    Try(0, 0);

    if (dem == 0)
        printf("Không có l?i gi?i\n");

    return 0;
}

