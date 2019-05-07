#include "fortran_wrapper.hpp"

// This is a helper function:
void removeEndSpaces(char *str) 
{ 
    int i;
    for (i = 0; str[i]; i++) 
        if(str[i] == ' ')
            break;

    str[i] = '\0'; 
} 

void initialize_kernel_object_c(int N, Kernel** kernel)
{
    (*kernel) = new Kernel(N);
    return;
}

void get_matrix_c(Kernel** kernel, int row_start, int col_start, int row_end, int col_end, double* matrix)
{
    Mat temp = (*kernel)->getMatrix(row_start, col_start, row_end, col_end);
    // Counter variables:
    int i, j;
    for(i = 0; i < temp.cols(); i++)
        for(j = 0; j < temp.rows(); j++)
            matrix[temp.rows() * i + j] = temp(j, i);

    return;
}

void initialize_matrix_factorizer_c(Kernel** kernel, char* factorization_type, Matrix_Factorizer** factorizer)
{   
    removeEndSpaces(factorization_type);
    (*factorizer) = new Matrix_Factorizer((*kernel), factorization_type);
    return;
}

void get_factorization_c(Matrix_Factorizer** factorizer, double eps, double* l, double* r, int& rank)
{
    Mat U, V;
    (*factorizer)->getFactorization(U, V, eps);

    rank = U.cols();
    // Counter variables:
    int i, j;
    for(i = 0; i < U.cols(); i++)
        for(j = 0; j < U.rows(); j++)
            l[U.rows() * i + j] = U(j, i);

    for(i = 0; i < V.cols(); i++)
        for(j = 0; j < V.rows(); j++)
            r[V.rows() * i + j] = V(j, i);

    return;
}

void initialize_hodlr_tree_c(int n_levels, double eps, Matrix_Factorizer** factorizer, HODLR_Tree** tree)
{
    (*tree) = new HODLR_Tree(n_levels, eps, (*factorizer));
    return;
}

void assemble_tree_c(HODLR_Tree** tree, bool is_sym, bool is_pd)
{
    (*tree)->assembleTree(is_sym, is_pd);
    return;
}

void matmat_product_c(HODLR_Tree** tree, double* x, double* b)
{
    Mat b_eig, x_eig;
    x_eig = Eigen::Map<Mat>(x, (*tree)->N, 1);
    b_eig = (*tree)->matmatProduct(x_eig);

    for(int i = 0; i < (*tree)->N; i++)
        b[i] = b_eig(i, 0);

    return;
}

void factorize_c(HODLR_Tree** tree)
{
    (*tree)->factorize();
    return;
}

void solve_c(HODLR_Tree** tree, double* b, double* x)
{
    Mat b_eig, x_eig;

    b_eig = Eigen::Map<Mat>(b, (*tree)->N, 1);
    x_eig = (*tree)->solve(b_eig);

    for(int i = 0; i < (*tree)->N; i++)
        x[i] = x_eig(i, 0);

    return;
}

void logdeterminant_c(HODLR_Tree** tree, double &log_det)
{
    log_det = (*tree)->logDeterminant();
    return;
}

void symm_factor_transpose_prod_c(HODLR_Tree** tree, double* x, double* b)
{
    Mat b_eig, x_eig;
    x_eig = Eigen::Map<Mat>(x, (*tree)->N, 1);
    b_eig = (*tree)->symmetricFactorTransposeProduct(x_eig);

    for(int i = 0; i < (*tree)->N; i++)
        b[i] = b_eig(i, 0);

    return;
}

void symm_factor_prod_c(HODLR_Tree** tree, double* x, double* b)
{
    Mat b_eig, x_eig;
    x_eig = Eigen::Map<Mat>(x, (*tree)->N, 1);
    b_eig = (*tree)->symmetricFactorProduct(x_eig);

    for(int i = 0; i < (*tree)->N; i++)
        b[i] = b_eig(i, 0);

    return;
}

void get_symm_factor_c(HODLR_Tree** tree, double* W_matrix)
{
    Mat temp = (*tree)->getSymmetricFactor();
    // Counter variables:
    int i, j;
    for(i = 0; i < temp.cols(); i++)
        for(j = 0; j < temp.rows(); j++)
            W_matrix[temp.rows() * i + j] = temp(j, i);

    return;
}