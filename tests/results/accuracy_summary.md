# CPU Wavelet Accuracy Summary

Classification uses thresholds as `wav/usage/test.py`:

- `green`: `avg_worst_mse < 5e-6`
- `yellow`: `avg_worst_mse < 1e-2`
- `red`: otherwise

Wavelets: 98
OK cases: 6174
Error cases: 0
Classes: green=72, yellow=6, red=20

| Wavelet | Avg cA MSE | Avg cD MSE | Avg worst MSE | Wavelet | Avg cA MSE | Avg cD MSE | Avg worst MSE | Wavelet | Avg cA MSE | Avg cD MSE | Avg worst MSE |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| bior1.1 | $${\color{green} \text{1.14e-15}}$$ | $${\color{green} \text{7.69e-16}}$$ | $${\color{green} \text{1.39e-15}}$$ | bior1.3 | $${\color{green} \text{1.51e-15}}$$ | $${\color{green} \text{4.61e-16}}$$ | $${\color{green} \text{1.65e-15}}$$ | bior1.5 | $${\color{green} \text{2.56e-15}}$$ | $${\color{green} \text{5.31e-16}}$$ | $${\color{green} \text{2.60e-15}}$$ |
| bior2.2 | $${\color{green} \text{2.51e-15}}$$ | $${\color{green} \text{5.10e-16}}$$ | $${\color{green} \text{2.56e-15}}$$ | bior2.4 | $${\color{green} \text{7.05e-15}}$$ | $${\color{green} \text{5.26e-16}}$$ | $${\color{green} \text{7.09e-15}}$$ | bior2.6 | $${\color{green} \text{8.44e-15}}$$ | $${\color{green} \text{6.28e-16}}$$ | $${\color{green} \text{8.46e-15}}$$ |
| bior2.8 | $${\color{green} \text{8.79e-15}}$$ | $${\color{green} \text{6.27e-16}}$$ | $${\color{green} \text{8.86e-15}}$$ | bior3.1 | $${\color{green} \text{8.34e-15}}$$ | $${\color{green} \text{2.38e-15}}$$ | $${\color{green} \text{9.06e-15}}$$ | bior3.3 | $${\color{green} \text{7.00e-15}}$$ | $${\color{green} \text{2.38e-15}}$$ | $${\color{green} \text{8.11e-15}}$$ |
| bior3.5 | $${\color{green} \text{2.56e-14}}$$ | $${\color{green} \text{3.81e-15}}$$ | $${\color{green} \text{2.58e-14}}$$ | bior3.7 | $${\color{green} \text{1.12e-14}}$$ | $${\color{green} \text{3.65e-15}}$$ | $${\color{green} \text{1.19e-14}}$$ | bior3.9 | $${\color{green} \text{1.26e-14}}$$ | $${\color{green} \text{3.47e-15}}$$ | $${\color{green} \text{1.31e-14}}$$ |
| bior4.4 | $${\color{green} \text{1.94e-14}}$$ | $${\color{green} \text{1.16e-14}}$$ | $${\color{green} \text{1.97e-14}}$$ | bior5.5 | $${\color{green} \text{3.94e-14}}$$ | $${\color{green} \text{7.92e-14}}$$ | $${\color{green} \text{7.96e-14}}$$ | bior6.8 | $${\color{green} \text{1.31e-14}}$$ | $${\color{green} \text{3.58e-15}}$$ | $${\color{green} \text{1.32e-14}}$$ |
| coif1 | $${\color{green} \text{4.12e-14}}$$ | $${\color{green} \text{1.59e-13}}$$ | $${\color{green} \text{1.63e-13}}$$ | coif2 | $${\color{green} \text{2.54e-13}}$$ | $${\color{green} \text{9.42e-15}}$$ | $${\color{green} \text{2.55e-13}}$$ | coif3 | $${\color{green} \text{1.34e-14}}$$ | $${\color{green} \text{1.73e-11}}$$ | $${\color{green} \text{1.73e-11}}$$ |
| coif4 | $${\color{green} \text{6.16e-12}}$$ | $${\color{green} \text{6.19e-14}}$$ | $${\color{green} \text{6.18e-12}}$$ | coif5 | $${\color{green} \text{2.37e-14}}$$ | $${\color{green} \text{8.79e-09}}$$ | $${\color{green} \text{8.79e-09}}$$ | coif6 | $${\color{green} \text{6.63e-10}}$$ | $${\color{green} \text{5.69e-14}}$$ | $${\color{green} \text{6.63e-10}}$$ |
| coif7 | $${\color{green} \text{8.53e-14}}$$ | $${\color{green} \text{2.47e-06}}$$ | $${\color{green} \text{2.47e-06}}$$ | coif8 | $${\color{green} \text{1.92e-07}}$$ | $${\color{green} \text{3.09e-13}}$$ | $${\color{green} \text{1.92e-07}}$$ | coif9 | $${\color{green} \text{1.14e-10}}$$ | $${\color{yellow} \text{1.58e-03}}$$ | $${\color{yellow} \text{1.58e-03}}$$ |
| db1 | $${\color{green} \text{1.14e-15}}$$ | $${\color{green} \text{7.69e-16}}$$ | $${\color{green} \text{1.39e-15}}$$ | db2 | $${\color{green} \text{5.04e-15}}$$ | $${\color{green} \text{1.58e-15}}$$ | $${\color{green} \text{5.38e-15}}$$ | db3 | $${\color{green} \text{8.37e-15}}$$ | $${\color{green} \text{1.26e-13}}$$ | $${\color{green} \text{1.28e-13}}$$ |
| db4 | $${\color{green} \text{6.47e-14}}$$ | $${\color{green} \text{5.33e-15}}$$ | $${\color{green} \text{6.52e-14}}$$ | db5 | $${\color{green} \text{1.87e-14}}$$ | $${\color{green} \text{2.38e-11}}$$ | $${\color{green} \text{2.38e-11}}$$ | db6 | $${\color{green} \text{2.87e-13}}$$ | $${\color{green} \text{6.67e-15}}$$ | $${\color{green} \text{2.87e-13}}$$ |
| db7 | $${\color{green} \text{2.70e-14}}$$ | $${\color{green} \text{1.04e-10}}$$ | $${\color{green} \text{1.04e-10}}$$ | db8 | $${\color{green} \text{6.14e-12}}$$ | $${\color{green} \text{1.29e-14}}$$ | $${\color{green} \text{6.14e-12}}$$ | db9 | $${\color{green} \text{2.79e-14}}$$ | $${\color{green} \text{2.88e-09}}$$ | $${\color{green} \text{2.88e-09}}$$ |
| db10 | $${\color{green} \text{3.01e-11}}$$ | $${\color{green} \text{1.09e-14}}$$ | $${\color{green} \text{3.01e-11}}$$ | db11 | $${\color{green} \text{8.48e-14}}$$ | $${\color{green} \text{9.85e-08}}$$ | $${\color{green} \text{9.85e-08}}$$ | db12 | $${\color{green} \text{6.34e-10}}$$ | $${\color{green} \text{1.54e-14}}$$ | $${\color{green} \text{6.34e-10}}$$ |
| db13 | $${\color{green} \text{3.31e-13}}$$ | $${\color{green} \text{7.77e-07}}$$ | $${\color{green} \text{7.77e-07}}$$ | db14 | $${\color{green} \text{1.30e-08}}$$ | $${\color{green} \text{3.72e-09}}$$ | $${\color{green} \text{1.31e-08}}$$ | db15 | $${\color{yellow} \text{9.66e-06}}$$ | $${\color{yellow} \text{4.31e-05}}$$ | $${\color{yellow} \text{4.33e-05}}$$ |
| db16 | $${\color{yellow} \text{4.55e-04}}$$ | $${\color{yellow} \text{4.11e-04}}$$ | $${\color{yellow} \text{4.64e-04}}$$ | db17 | $${\color{red} \text{1.01e-02}}$$ | $${\color{yellow} \text{9.23e-03}}$$ | $${\color{red} \text{1.03e-02}}$$ | db18 | $${\color{yellow} \text{9.02e-03}}$$ | $${\color{yellow} \text{8.59e-03}}$$ | $${\color{yellow} \text{9.23e-03}}$$ |
| db19 | $${\color{yellow} \text{7.70e-03}}$$ | $${\color{red} \text{1.54e-02}}$$ | $${\color{red} \text{1.55e-02}}$$ | db20 | $${\color{yellow} \text{6.78e-03}}$$ | $${\color{yellow} \text{7.61e-03}}$$ | $${\color{yellow} \text{7.67e-03}}$$ | db21 | $${\color{yellow} \text{6.18e-03}}$$ | $${\color{red} \text{1.00e-01}}$$ | $${\color{red} \text{1.00e-01}}$$ |
| db22 | $${\color{yellow} \text{6.66e-03}}$$ | $${\color{yellow} \text{6.70e-03}}$$ | $${\color{yellow} \text{7.09e-03}}$$ | db23 | $${\color{yellow} \text{5.67e-03}}$$ | $${\color{red} \text{4.09e-01}}$$ | $${\color{red} \text{4.10e-01}}$$ | db24 | $${\color{red} \text{1.25e-02}}$$ | $${\color{yellow} \text{5.07e-03}}$$ | $${\color{red} \text{1.26e-02}}$$ |
| db25 | $${\color{yellow} \text{4.91e-03}}$$ | $${\color{red} \text{2.05e+01}}$$ | $${\color{red} \text{2.05e+01}}$$ | db26 | $${\color{red} \text{6.84e-02}}$$ | $${\color{yellow} \text{4.35e-03}}$$ | $${\color{red} \text{6.85e-02}}$$ | db27 | $${\color{yellow} \text{2.64e-03}}$$ | $${\color{red} \text{1.33e+02}}$$ | $${\color{red} \text{1.33e+02}}$$ |
| db28 | $${\color{red} \text{1.51e+00}}$$ | $${\color{red} \text{1.56e-02}}$$ | $${\color{red} \text{1.51e+00}}$$ | db29 | $${\color{red} \text{1.61e-02}}$$ | $${\color{red} \text{1.86e+04}}$$ | $${\color{red} \text{1.86e+04}}$$ | db30 | $${\color{red} \text{5.06e+00}}$$ | $${\color{red} \text{1.41e-02}}$$ | $${\color{red} \text{5.06e+00}}$$ |
| db31 | $${\color{red} \text{1.32e-02}}$$ | $${\color{red} \text{1.96e+05}}$$ | $${\color{red} \text{1.96e+05}}$$ | db32 | $${\color{red} \text{2.13e+01}}$$ | $${\color{red} \text{1.19e-02}}$$ | $${\color{red} \text{2.13e+01}}$$ | db33 | $${\color{red} \text{1.22e-02}}$$ | $${\color{red} \text{3.51e+05}}$$ | $${\color{red} \text{3.51e+05}}$$ |
| db34 | $${\color{red} \text{2.64e+03}}$$ | $${\color{red} \text{1.10e-02}}$$ | $${\color{red} \text{2.64e+03}}$$ | db35 | $${\color{red} \text{1.03e-02}}$$ | $${\color{red} \text{7.30e+06}}$$ | $${\color{red} \text{7.30e+06}}$$ | db36 | $${\color{red} \text{3.19e+03}}$$ | $${\color{red} \text{1.00e-02}}$$ | $${\color{red} \text{3.19e+03}}$$ |
| db37 | $${\color{red} \text{5.86e-02}}$$ | $${\color{red} \text{1.52e+08}}$$ | $${\color{red} \text{1.52e+08}}$$ | db38 | $${\color{red} \text{2.45e+05}}$$ | $${\color{red} \text{1.85e-02}}$$ | $${\color{red} \text{2.45e+05}}$$ | dmey | $${\color{red} \text{1.83e-01}}$$ | $${\color{red} \text{1.90e-01}}$$ | $${\color{red} \text{1.91e-01}}$$ |
| haar | $${\color{green} \text{1.14e-15}}$$ | $${\color{green} \text{7.69e-16}}$$ | $${\color{green} \text{1.39e-15}}$$ | rbio1.1 | $${\color{green} \text{1.14e-15}}$$ | $${\color{green} \text{7.69e-16}}$$ | $${\color{green} \text{1.39e-15}}$$ | rbio1.3 | $${\color{green} \text{1.01e-15}}$$ | $${\color{green} \text{1.04e-15}}$$ | $${\color{green} \text{1.49e-15}}$$ |
| rbio1.5 | $${\color{green} \text{1.06e-15}}$$ | $${\color{green} \text{1.60e-15}}$$ | $${\color{green} \text{1.97e-15}}$$ | rbio2.2 | $${\color{green} \text{8.43e-16}}$$ | $${\color{green} \text{1.69e-15}}$$ | $${\color{green} \text{1.80e-15}}$$ | rbio2.4 | $${\color{green} \text{9.34e-16}}$$ | $${\color{green} \text{4.34e-15}}$$ | $${\color{green} \text{4.43e-15}}$$ |
| rbio2.6 | $${\color{green} \text{9.15e-16}}$$ | $${\color{green} \text{6.43e-15}}$$ | $${\color{green} \text{6.51e-15}}$$ | rbio2.8 | $${\color{green} \text{9.39e-16}}$$ | $${\color{green} \text{6.06e-15}}$$ | $${\color{green} \text{6.11e-15}}$$ | rbio3.1 | $${\color{green} \text{4.83e-15}}$$ | $${\color{green} \text{6.22e-15}}$$ | $${\color{green} \text{8.87e-15}}$$ |
| rbio3.3 | $${\color{green} \text{4.82e-15}}$$ | $${\color{green} \text{7.69e-15}}$$ | $${\color{green} \text{1.06e-14}}$$ | rbio3.5 | $${\color{green} \text{4.76e-15}}$$ | $${\color{green} \text{2.66e-14}}$$ | $${\color{green} \text{2.66e-14}}$$ | rbio3.7 | $${\color{green} \text{4.76e-15}}$$ | $${\color{green} \text{1.20e-14}}$$ | $${\color{green} \text{1.40e-14}}$$ |
| rbio3.9 | $${\color{green} \text{4.73e-15}}$$ | $${\color{green} \text{1.44e-14}}$$ | $${\color{green} \text{1.65e-14}}$$ | rbio4.4 | $${\color{green} \text{1.92e-14}}$$ | $${\color{green} \text{2.94e-14}}$$ | $${\color{green} \text{3.02e-14}}$$ | rbio5.5 | $${\color{green} \text{6.68e-14}}$$ | $${\color{green} \text{3.19e-14}}$$ | $${\color{green} \text{6.84e-14}}$$ |
| rbio6.8 | $${\color{green} \text{5.90e-15}}$$ | $${\color{green} \text{1.79e-14}}$$ | $${\color{green} \text{1.82e-14}}$$ | sym2 | $${\color{green} \text{5.04e-15}}$$ | $${\color{green} \text{1.58e-15}}$$ | $${\color{green} \text{5.38e-15}}$$ | sym3 | $${\color{green} \text{8.37e-15}}$$ | $${\color{green} \text{1.26e-13}}$$ | $${\color{green} \text{1.28e-13}}$$ |
| sym4 | $${\color{green} \text{1.67e-14}}$$ | $${\color{green} \text{5.64e-14}}$$ | $${\color{green} \text{5.82e-14}}$$ | sym5 | $${\color{green} \text{1.96e-14}}$$ | $${\color{green} \text{9.47e-15}}$$ | $${\color{green} \text{2.24e-14}}$$ | sym6 | $${\color{green} \text{4.27e-14}}$$ | $${\color{green} \text{5.56e-14}}$$ | $${\color{green} \text{6.31e-14}}$$ |
| sym7 | $${\color{green} \text{4.83e-11}}$$ | $${\color{green} \text{1.12e-10}}$$ | $${\color{green} \text{1.12e-10}}$$ | sym8 | $${\color{green} \text{6.11e-14}}$$ | $${\color{green} \text{3.77e-14}}$$ | $${\color{green} \text{6.98e-14}}$$ | sym9 | $${\color{green} \text{3.99e-13}}$$ | $${\color{green} \text{2.19e-13}}$$ | $${\color{green} \text{4.00e-13}}$$ |
| sym10 | $${\color{green} \text{8.02e-14}}$$ | $${\color{green} \text{1.37e-13}}$$ | $${\color{green} \text{1.40e-13}}$$ | sym11 | $${\color{green} \text{5.56e-13}}$$ | $${\color{green} \text{7.16e-13}}$$ | $${\color{green} \text{7.60e-13}}$$ | sym12 | $${\color{green} \text{6.87e-14}}$$ | $${\color{green} \text{9.05e-14}}$$ | $${\color{green} \text{1.01e-13}}$$ |
| sym13 | $${\color{green} \text{4.24e-10}}$$ | $${\color{green} \text{7.79e-11}}$$ | $${\color{green} \text{4.24e-10}}$$ | sym14 | $${\color{green} \text{1.21e-12}}$$ | $${\color{green} \text{7.54e-12}}$$ | $${\color{green} \text{7.58e-12}}$$ | sym15 | $${\color{green} \text{9.14e-14}}$$ | $${\color{green} \text{2.98e-13}}$$ | $${\color{green} \text{2.99e-13}}$$ |
| sym16 | $${\color{green} \text{4.53e-14}}$$ | $${\color{green} \text{2.68e-14}}$$ | $${\color{green} \text{5.04e-14}}$$ | sym17 | $${\color{green} \text{5.59e-14}}$$ | $${\color{green} \text{2.65e-14}}$$ | $${\color{green} \text{6.06e-14}}$$ | sym18 | $${\color{green} \text{3.50e-14}}$$ | $${\color{green} \text{3.17e-14}}$$ | $${\color{green} \text{4.20e-14}}$$ |
| sym19 | $${\color{green} \text{1.62e-13}}$$ | $${\color{green} \text{2.35e-13}}$$ | $${\color{green} \text{3.33e-13}}$$ | sym20 | $${\color{green} \text{5.40e-13}}$$ | $${\color{green} \text{6.55e-13}}$$ | $${\color{green} \text{7.54e-13}}$$ |  |  |  |  |

## Max Worst MSE

| Wavelet | Max worst MSE | OK cases | Error cases |
| --- | --- | --- | --- |
| bior1.1 | $${\color{green} \text{1.44e-14}}$$ | 63 | 0 |
| bior1.3 | $${\color{green} \text{7.41e-15}}$$ | 63 | 0 |
| bior1.5 | $${\color{green} \text{1.12e-14}}$$ | 63 | 0 |
| bior2.2 | $${\color{green} \text{9.45e-15}}$$ | 63 | 0 |
| bior2.4 | $${\color{green} \text{2.49e-14}}$$ | 63 | 0 |
| bior2.6 | $${\color{green} \text{3.71e-14}}$$ | 63 | 0 |
| bior2.8 | $${\color{green} \text{4.45e-14}}$$ | 63 | 0 |
| bior3.1 | $${\color{green} \text{2.65e-14}}$$ | 63 | 0 |
| bior3.3 | $${\color{green} \text{3.77e-14}}$$ | 63 | 0 |
| bior3.5 | $${\color{green} \text{6.47e-14}}$$ | 63 | 0 |
| bior3.7 | $${\color{green} \text{3.14e-14}}$$ | 63 | 0 |
| bior3.9 | $${\color{green} \text{8.18e-14}}$$ | 63 | 0 |
| bior4.4 | $${\color{green} \text{1.02e-13}}$$ | 63 | 0 |
| bior5.5 | $${\color{green} \text{3.73e-13}}$$ | 63 | 0 |
| bior6.8 | $${\color{green} \text{4.14e-14}}$$ | 63 | 0 |
| coif1 | $${\color{green} \text{4.62e-13}}$$ | 63 | 0 |
| coif2 | $${\color{green} \text{2.67e-12}}$$ | 63 | 0 |
| coif3 | $${\color{green} \text{8.27e-11}}$$ | 63 | 0 |
| coif4 | $${\color{green} \text{4.06e-11}}$$ | 63 | 0 |
| coif5 | $${\color{green} \text{3.49e-08}}$$ | 63 | 0 |
| coif6 | $${\color{green} \text{6.01e-09}}$$ | 63 | 0 |
| coif7 | $${\color{yellow} \text{1.28e-05}}$$ | 63 | 0 |
| coif8 | $${\color{green} \text{1.84e-06}}$$ | 63 | 0 |
| coif9 | $${\color{yellow} \text{5.33e-03}}$$ | 63 | 0 |
| db1 | $${\color{green} \text{1.44e-14}}$$ | 63 | 0 |
| db2 | $${\color{green} \text{1.42e-14}}$$ | 63 | 0 |
| db3 | $${\color{green} \text{4.70e-13}}$$ | 63 | 0 |
| db4 | $${\color{green} \text{4.37e-13}}$$ | 63 | 0 |
| db5 | $${\color{green} \text{1.21e-10}}$$ | 63 | 0 |
| db6 | $${\color{green} \text{1.79e-12}}$$ | 63 | 0 |
| db7 | $${\color{green} \text{5.04e-10}}$$ | 63 | 0 |
| db8 | $${\color{green} \text{5.76e-11}}$$ | 63 | 0 |
| db9 | $${\color{green} \text{9.04e-09}}$$ | 63 | 0 |
| db10 | $${\color{green} \text{2.32e-10}}$$ | 63 | 0 |
| db11 | $${\color{green} \text{2.51e-07}}$$ | 63 | 0 |
| db12 | $${\color{green} \text{3.26e-09}}$$ | 63 | 0 |
| db13 | $${\color{green} \text{3.37e-06}}$$ | 63 | 0 |
| db14 | $${\color{green} \text{7.81e-08}}$$ | 63 | 0 |
| db15 | $${\color{yellow} \text{1.44e-04}}$$ | 63 | 0 |
| db16 | $${\color{yellow} \text{1.16e-03}}$$ | 63 | 0 |
| db17 | $${\color{red} \text{2.41e-02}}$$ | 63 | 0 |
| db18 | $${\color{red} \text{1.99e-02}}$$ | 63 | 0 |
| db19 | $${\color{red} \text{4.02e-02}}$$ | 63 | 0 |
| db20 | $${\color{red} \text{1.70e-02}}$$ | 63 | 0 |
| db21 | $${\color{red} \text{4.38e-01}}$$ | 63 | 0 |
| db22 | $${\color{red} \text{2.12e-02}}$$ | 63 | 0 |
| db23 | $${\color{red} \text{2.34e+00}}$$ | 63 | 0 |
| db24 | $${\color{red} \text{5.90e-02}}$$ | 63 | 0 |
| db25 | $${\color{red} \text{1.12e+02}}$$ | 63 | 0 |
| db26 | $${\color{red} \text{3.41e-01}}$$ | 63 | 0 |
| db27 | $${\color{red} \text{7.74e+02}}$$ | 63 | 0 |
| db28 | $${\color{red} \text{9.19e+00}}$$ | 63 | 0 |
| db29 | $${\color{red} \text{6.10e+04}}$$ | 63 | 0 |
| db30 | $${\color{red} \text{3.16e+01}}$$ | 63 | 0 |
| db31 | $${\color{red} \text{8.17e+05}}$$ | 63 | 0 |
| db32 | $${\color{red} \text{1.60e+02}}$$ | 63 | 0 |
| db33 | $${\color{red} \text{1.60e+06}}$$ | 63 | 0 |
| db34 | $${\color{red} \text{1.61e+04}}$$ | 63 | 0 |
| db35 | $${\color{red} \text{4.75e+07}}$$ | 63 | 0 |
| db36 | $${\color{red} \text{4.64e+04}}$$ | 63 | 0 |
| db37 | $${\color{red} \text{1.02e+09}}$$ | 63 | 0 |
| db38 | $${\color{red} \text{2.52e+06}}$$ | 63 | 0 |
| dmey | $${\color{red} \text{3.77e-01}}$$ | 63 | 0 |
| haar | $${\color{green} \text{1.44e-14}}$$ | 63 | 0 |
| rbio1.1 | $${\color{green} \text{1.44e-14}}$$ | 63 | 0 |
| rbio1.3 | $${\color{green} \text{9.77e-15}}$$ | 63 | 0 |
| rbio1.5 | $${\color{green} \text{1.44e-14}}$$ | 63 | 0 |
| rbio2.2 | $${\color{green} \text{1.09e-14}}$$ | 63 | 0 |
| rbio2.4 | $${\color{green} \text{2.94e-14}}$$ | 63 | 0 |
| rbio2.6 | $${\color{green} \text{5.78e-14}}$$ | 63 | 0 |
| rbio2.8 | $${\color{green} \text{3.41e-14}}$$ | 63 | 0 |
| rbio3.1 | $${\color{green} \text{4.85e-14}}$$ | 63 | 0 |
| rbio3.3 | $${\color{green} \text{4.33e-14}}$$ | 63 | 0 |
| rbio3.5 | $${\color{green} \text{1.14e-13}}$$ | 63 | 0 |
| rbio3.7 | $${\color{green} \text{4.03e-14}}$$ | 63 | 0 |
| rbio3.9 | $${\color{green} \text{7.91e-14}}$$ | 63 | 0 |
| rbio4.4 | $${\color{green} \text{1.03e-13}}$$ | 63 | 0 |
| rbio5.5 | $${\color{green} \text{2.16e-13}}$$ | 63 | 0 |
| rbio6.8 | $${\color{green} \text{4.88e-14}}$$ | 63 | 0 |
| sym2 | $${\color{green} \text{1.42e-14}}$$ | 63 | 0 |
| sym3 | $${\color{green} \text{4.70e-13}}$$ | 63 | 0 |
| sym4 | $${\color{green} \text{1.75e-13}}$$ | 63 | 0 |
| sym5 | $${\color{green} \text{7.46e-14}}$$ | 63 | 0 |
| sym6 | $${\color{green} \text{3.05e-13}}$$ | 63 | 0 |
| sym7 | $${\color{green} \text{9.58e-10}}$$ | 63 | 0 |
| sym8 | $${\color{green} \text{2.25e-13}}$$ | 63 | 0 |
| sym9 | $${\color{green} \text{1.19e-12}}$$ | 63 | 0 |
| sym10 | $${\color{green} \text{3.95e-13}}$$ | 63 | 0 |
| sym11 | $${\color{green} \text{2.47e-12}}$$ | 63 | 0 |
| sym12 | $${\color{green} \text{5.40e-13}}$$ | 63 | 0 |
| sym13 | $${\color{green} \text{1.33e-09}}$$ | 63 | 0 |
| sym14 | $${\color{green} \text{3.60e-11}}$$ | 63 | 0 |
| sym15 | $${\color{green} \text{7.79e-13}}$$ | 63 | 0 |
| sym16 | $${\color{green} \text{1.43e-13}}$$ | 63 | 0 |
| sym17 | $${\color{green} \text{2.78e-13}}$$ | 63 | 0 |
| sym18 | $${\color{green} \text{1.36e-13}}$$ | 63 | 0 |
| sym19 | $${\color{green} \text{1.07e-12}}$$ | 63 | 0 |
| sym20 | $${\color{green} \text{2.47e-12}}$$ | 63 | 0 |
