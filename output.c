#include <stdbool.h>
#include <stdio.h>

#include <memory.h>

void BubbleSort(int n, float  A[n - 1 + 1]) {
    const int A_offset_0 = 1 - 1;
    int i;
    int j;
    float temp;
    bool ordinato;
    ordinato = false;
    i = 1;
    while ((((i < n) && !ordinato))) {
        ordinato = false;
        for (j = 1; j <= (n - i); j += 1) {
            if (((A[j] > A[(j + 1)]))) {
                temp = A[(j - 1 - A_offset_0)];
                A[(j - 1 - A_offset_0)] = A[((j + 1) - 1 - A_offset_0)];
                A[(((j + 1)) - 1 - A_offset_0)] = temp;
                ordinato = false;
            }
        }
        i = (i + 1);
    }
}

void InsertionSort(int n, int  arr[n]) {
    int i;
    int j;
    int key;
    for (i = 1; i <= (n - 1); i += 1) {
        key = arr[(i - 1)];
        j = (i - 1);
        while ((((j >= 0) && (arr[j] > key)))) {
            arr[(((j + 1)) - 1)] = arr[(j - 1)];
            j = (j - 1);
        }
        arr[(((j + 1)) - 1)] = key;
        i = 0xFF34;
        i = 03436;
        i = 0b0101010001;
        i = 034243;
    }
}

void SelectionSort2(int n, float  A[n - 1 + 1]) {
    const int A_offset_0 = 1 - 1;
    int i;
    int j;
    int p;
    float min;
    float temp;
    for (i = 1; i <= (n - 1); i += 1) {
        min = A[(i - 1 - A_offset_0)];
        p = i;
        for (j = (i + 1); j <= n; j += 1) {
            if (((A[j] < min))) {
                min = A[(j - 1 - A_offset_0)];
                p = j;
            }
        }
        temp = A[(i - 1 - A_offset_0)];
        A[(i - 1 - A_offset_0)] = A[(p - 1 - A_offset_0)];
        A[(p - 1 - A_offset_0)] = temp;
    }
}

void MergeSort(int  arr[], int l, int r) {
    int m;
    if (((l < r))) {
        m = ((l + r) / 2);
        MergeSort(arr, l, m);
        MergeSort(arr, ((m + 1)), r);
        Merge(arr, l, m, r);
    }
}

void Merge(int  arr[], int l, int m, int r) {
    int n1;
    int n2;
    int i;
    int j;
    int k;
    int L[((m - l) + 1)];
    int R[(r - m)];
    n1 = ((m - l) + 1);
    n2 = (r - m);
    for (i = 0; i <= (n1 - 1); i += 1) {
        L[(i - 1)] = arr[((l + i) - 1)];
    }
    for (j = 0; j <= (n2 - 1); j += 1) {
        R[(j - 1)] = arr[(((m + 1) + j) - 1)];
    }
    i = 0;
    j = 0;
    k = l;
    while ((((i < n1) && (j < n2)))) {
        if (((L[i] <= R[j]))) {
            arr[(k - 1)] = L[(i - 1)];
            i = (i + 1);
        } else {
            arr[(k - 1)] = R[(j - 1)];
            j = (j + 1);
        }
        k = (k + 1);
    }
    while (((i < n1))) {
        arr[(k - 1)] = L[(i - 1)];
        i = (i + 1);
        k = (k + 1);
    }
    while (((j < n2))) {
        arr[(k - 1)] = R[(j - 1)];
        j = (j + 1);
        k = (k + 1);
    }
}

float ScalarProductVdotV(float  x[n - 1 + 1], float  y[n - 1 + 1], int n) {
    float ScalarProductVdotV;
    const int x_offset_0 = 1 - 1;
    const int y_offset_0 = 1 - 1;
    int i;
    ScalarProductVdotV = 0.0;
    for (i = 1; i <= n; i += 1) {
        ScalarProductVdotV = (ScalarProductVdotV + (x[i] * y[i]));
    }
    return ScalarProductVdotV;
}

void saxpy(float a, float  x[n - 1 + 1], int n, float  y[n - 1 + 1]) {
    const int x_offset_0 = 1 - 1;
    const int y_offset_0 = 1 - 1;
    int i;
    for (i = 1; i <= n; i += 1) {
        y[(i - 1 - y_offset_0)] = (y[i] + ax(i));
    }
}

void MatrixSum(int m, int n, float  A[m - 1 + 1][n - 1 + 1], float  B[m - 1 + 1][n - 1 + 1], float  C[m - 1 + 1][n - 1 + 1]) {
    const int A_offset_0 = 1 - 1;
    const int A_offset_1 = 1 - 1;
    const int B_offset_0 = 1 - 1;
    const int B_offset_1 = 1 - 1;
    const int C_offset_0 = 1 - 1;
    const int C_offset_1 = 1 - 1;
    int i;
    int j;
    for (i = 1; i <= m; i += 1) {
        for (j = 1; j <= n; j += 1) {
            C[(i - 1 - C_offset_0)][(j - 1 - C_offset_1)] = (A[i] + B[i]);
        }
    }
}

void ScalarProductMdotV(int m, int n, float  A[m - 1 + 1][n - 1 + 1], float  y[n - 1 + 1], float  z[m - 1 + 1]) {
    const int A_offset_0 = 1 - 1;
    const int A_offset_1 = 1 - 1;
    const int y_offset_0 = 1 - 1;
    const int z_offset_0 = 1 - 1;
    int i;
    int j;
    for (i = 1; i <= m; i += 1) {
        z[(i - 1 - z_offset_0)] = 0;
        for (j = 1; j <= n; j += 1) {
            z[(i - 1 - z_offset_0)] = (z[i] + (A[i] * y[j]));
        }
    }
}

void ScalarProductMdotM(int m, int n, int p, float  A[m - 1 + 1][n - 1 + 1], float  B[n - 1 + 1][p - 1 + 1], float  C[m - 1 + 1][p - 1 + 1]) {
    const int A_offset_0 = 1 - 1;
    const int A_offset_1 = 1 - 1;
    const int B_offset_0 = 1 - 1;
    const int B_offset_1 = 1 - 1;
    const int C_offset_0 = 1 - 1;
    const int C_offset_1 = 1 - 1;
    int i;
    int j;
    int k;
    for (i = 1; i <= m; i += 1) {
        for (j = 1; j <= p; j += 1) {
            C[(i - 1 - C_offset_0)][(j - 1 - C_offset_1)] = 0.0;
            for (k = 1; k <= n; k += 1) {
                C[(i - 1 - C_offset_0)][(j - 1 - C_offset_1)] = (C[i] + (A[i] * B[k]));
            }
        }
    }
}

void SelectionSort(int n, int  arr[n]) {
    int i;
    int j;
    int minIndex;
    int temp;
    for (i = 0; i <= (n - 2); i += 1) {
        minIndex = i;
        for (j = (i + 1); j <= (n - 1); j += 1) {
            if (((arr[j] < arr[minIndex]))) {
                minIndex = j;
            }
        }
        if (((minIndex != i))) {
            temp = arr[(i - 1)];
            arr[(i - 1)] = arr[(minIndex - 1)];
            arr[(minIndex - 1)] = temp;
        }
    }
}

bool OddAndEvenSum(int  A[N], int N) {
    bool OddAndEvenSum;
    int sum_pari;
    int sum_dispari;
    int i;
    int j;
    bool continua;
    i = 1;
    j = 1;
    continua = false;
    while ((((i <= N) && continua))) {
        sum_pari = 0;
        sum_dispari = 0;
        for (j = 1; j <= (N - 1); j += 2) {
            sum_dispari = (sum_dispari + A[i]);
            sum_pari = (sum_pari + A[i]);
        }
        if (((sum_pari != sum_dispari))) {
            continua = false;
        }
        i = (i + 1);
    }
    OddAndEvenSum = continua;
    foo();
    return OddAndEvenSum;
}

