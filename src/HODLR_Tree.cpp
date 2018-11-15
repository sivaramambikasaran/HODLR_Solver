#include<HODLR_Tree.hpp>

HODLR_Tree::HODLR_Tree(int nLevels, double tolerance, HODLR_Matrix* A) 
{
    // // std::cout << "\nStart HODLR_Tree\n";
    this->nLevels   =   nLevels;
    this->tolerance =   tolerance;
    this->A         =   A;
    this->N         =   A->N;
    nodesInLevel.push_back(1);
    for (int j=1; j<=nLevels; ++j) 
    {
        nodesInLevel.push_back(2*nodesInLevel.back());
    }
    // // std::cout << "\nDone HODLR_Tree\n";
    createTree();
}

/*******************************************************/
/*  PURPOSE OF EXISTENCE: Builds the root of HODLR Tree*/
/*******************************************************/

void HODLR_Tree::createRoot() {
    // // std::cout << "\nStart createRoot\n";
    HODLR_Node* root    =   new HODLR_Node(0, 0, 0, 0, N, tolerance);
    std::vector<HODLR_Node*> level;
    level.push_back(root);
    tree.push_back(level);
    // // std::cout << "\nDone createRoot\n";
}

/******************************************************************************/
/*  PURPOSE OF EXISTENCE: Builds HODLR child nodes for HODLR nodes in the tree*/
/******************************************************************************/

/************/
/*  INPUTS  */
/************/

/// j           -   Level number
/// k           -   Node number

void HODLR_Tree::createChildren(int j, int k) {
    // // std::cout << "\nStart createChildren\n";
    //  Adding left child
    HODLR_Node* left    =   new HODLR_Node(2*j, k+1, 0, tree[j][k]->cStart[0], tree[j][k]->cSize[0], tolerance);
    tree[j+1].push_back(left);

    //  Adding right child
    HODLR_Node* right   =   new HODLR_Node(2*j+1, k+1, 1, tree[j][k]->cStart[1], tree[j][k]->cSize[1], tolerance);
    tree[j+1].push_back(right);
    // // std::cout << "\nDone createChildren\n";
}

/****************************************************************/
/*  PURPOSE OF EXISTENCE: Builds HODLR nodes for HODLR Matrix A */
/****************************************************************/

void HODLR_Tree::createTree() {
    // // std::cout << "\nStart createTree\n";
    createRoot();
    for (int j=0; j<nLevels; ++j) {
        std::vector<HODLR_Node*> level;
        tree.push_back(level);
        for (int k=0; k<nodesInLevel[j]; ++k) {
            createChildren(j,k);
        }
    }
    // // std::cout << "\nDone createTree\n";
}

/************************************************************************************************************************************************/
/*  PURPOSE OF EXISTENCE: Obtains a factorization of the leaf nodes and computes the low rank approximations of the off-diagonal blocks, Z=UV'. */
/************************************************************************************************************************************************/

void HODLR_Tree::assemble_Tree() {
    // // std::cout << "\nStart assemble_Tree\n";
    for (int j=0; j<nLevels; ++j) {
        #pragma omp parallel for
        for (int k=0; k<nodesInLevel[j]; ++k) {
            tree[j][k]->assemble_Non_Leaf_Node(A);
        }
    }
    #pragma omp parallel for
    for (int k=0; k<nodesInLevel[nLevels]; ++k) {
        tree[nLevels][k]->assemble_Leaf_Node(A);
    }
    // // std::cout << "\nDone assemble_Tree\n";
}

/*******************************************************/
/*  PURPOSE OF EXISTENCE:    Matrix-matrix product     */
/*******************************************************/

/************/
/*  INPUTS  */
/************/

/// x   -   Matrix to be multiplied on the right of the HODLR matrix

/************/
/*  OUTPUTS */
/************/

/// b   -   Matrix matrix product

void HODLR_Tree::matmat_Product(Eigen::MatrixXd x, Eigen::MatrixXd& b) {
    // // std::cout << "\nStart matmat_Product\n";
    int r   =   x.cols();
    b       =   Eigen::MatrixXd::Zero(N,r);
    for (int j=0; j<nLevels; ++j) {
        #pragma omp parallel for
        for (int k=0; k<nodesInLevel[j]; ++k) {
            tree[j][k]->matmat_Product_Non_Leaf(x, b);
        }
    }
    #pragma omp parallel for
    for (int k=0; k<nodesInLevel[nLevels]; ++k) {
        tree[nLevels][k]->matmat_Product_Leaf(x, b);
    }
    // // std::cout << "\nDone matmat_Product\n";
}

//  Factorization begins from here

/****************************************************************************************/
/*  PURPOSE OF EXISTENCE:   Routine is used for obtaining factorisations of leaf nodes  */
/****************************************************************************************/

/************/
/*  INPUTS  */
/************/

/// k           -   Leaf Node number

void HODLR_Tree::factorize_Leaf(int k) {
    // std::cout << "\nStart factorize Leaf: " << k << "\n";
    tree[nLevels][k]->Kfactor.compute(tree[nLevels][k]->K);
    int parent  =   k;
    int child   =   k;
    int size    =   tree[nLevels][k]->nSize;
    int tstart, r;
    #pragma omp parallel for
    for (int l=nLevels-1; l>=0; --l) {
        child   =   parent%2;
        parent  =   parent/2;
        tstart  =   tree[nLevels][k]->nStart-tree[l][parent]->cStart[child];
        r       =   tree[l][parent]->rank[child];
        tree[l][parent]->Ufactor[child].block(tstart,0,size,r)  =   solve_Leaf(k, tree[l][parent]->Ufactor[child].block(tstart,0,size,r));
    }
}

/************************************************************************************/
/*  PURPOSE OF EXISTENCE:   Routine for solving Kx = b, where K is the leaf node k. */
/************************************************************************************/

/************/
/*  INPUTS  */
/************/

/// k           -   Node number
/// b           -   Input matrix

/************/
/*  OUTPUT  */
/************/

/// x           -   Inverse of K multiplied with b


Eigen::MatrixXd HODLR_Tree::solve_Leaf(int k, Eigen::MatrixXd b) {
    // std::cout << "\nStart solve Leaf: " << k << "\n";
    Eigen::MatrixXd x   =   tree[nLevels][k]->Kfactor.solve(b);
    // std::cout << "\nDone solve Leaf: " << k << "\n";
    return x;
}

/********************************************************************************************/
/*  PURPOSE OF EXISTENCE:   Routine is used for obtaining factorisations of non-leaf nodes  */
/********************************************************************************************/

/************/
/*  INPUTS  */
/************/

/// j           -   Level number
/// k           -   Node number

void HODLR_Tree::factorize_Non_Leaf(int j, int k) {
    // std::cout << "\nStart factorize; Level: " << j << "Node: " << k << "\n";
    int r0  =   tree[j][k]->rank[0];
    int r1  =   tree[j][k]->rank[1];
    tree[j][k]->K   =   Eigen::MatrixXd::Identity(r0+r1, r0+r1);
    // std::cout << tree[j][k]->K.rows() << "\t" << tree[j][k]->K.cols() << "\n";
    tree[j][k]->K.block(0, r0, r0, r1)  =   tree[j][k]->Vfactor[1].transpose()*tree[j][k]->Ufactor[1];
    tree[j][k]->K.block(r0, 0, r1, r0)  =   tree[j][k]->Vfactor[0].transpose()*tree[j][k]->Ufactor[0];
    tree[j][k]->Kfactor.compute(tree[j][k]->K);
    int parent  =   k;
    int child   =   k;
    int size    =   tree[j][k]->nSize;
    int tstart, r;

    for (int l=j-1; l>=0; --l) {
        child   =   parent%2;
        parent  =   parent/2;
        tstart  =   tree[j][k]->nStart-tree[l][parent]->cStart[child];
        r       =   tree[l][parent]->rank[child];
        // std::cout << "\n" << size << "\n";
        tree[l][parent]->Ufactor[child].block(tstart,0,size,r)  =   solve_Non_Leaf(j, k, tree[l][parent]->Ufactor[child].block(tstart,0,size,r));
    }
    // std::cout << "\nDone factorize; Level: " << j << "Node: " << k << "\n";
}

/********************************************************************************************************************************/
/*  PURPOSE OF EXISTENCE:   Routine for solving (I+UKV')x = b. The method uses Sherman-Morrison-Woodsbury formula to obtain x.  */
/********************************************************************************************************************************/

/************/
/*  INPUTS  */
/************/

/// j           -   Level number
/// k           -   Node number
/// b           -   Input matrix

/************/
/*  OUTPUT  */
/************/

/// matrix          -   (I-U(inverse of (I+K))KV') multiplied by b

Eigen::MatrixXd HODLR_Tree::solve_Non_Leaf(int j, int k, Eigen::MatrixXd b) {
    // std::cout << "\nStart Solve non leaf; Level: " << j << "Node: " << k << "\n";
    int r0  =   tree[j][k]->rank[0];
    int r1  =   tree[j][k]->rank[1];
    int n0  =   tree[j][k]->cSize[0];
    int n1  =   tree[j][k]->cSize[1];
    int r   =   b.cols();
    Eigen::MatrixXd temp(r0+r1, r);
    temp << tree[j][k]->Vfactor[1].transpose()*b.block(n0,0,n1,r),
         tree[j][k]->Vfactor[0].transpose()*b.block(0,0,n0,r);
    temp    =   tree[j][k]->Kfactor.solve(temp);
    Eigen::MatrixXd y(n0+n1, r);
    y << tree[j][k]->Ufactor[0]*temp.block(0,0,r0,r), tree[j][k]->Ufactor[1]*temp.block(r0,0,r1,r);
    return (b-y);
}

/*********************************************************/
/*  PURPOSE OF EXISTENCE: Factorises the kernel matrix A.*/
/*********************************************************/

void HODLR_Tree::factorize() {
    // std::cout << "\nStart factorize...\n";
    //  Set things for factorization
    // std::cout << "Number of levels: " << nLevels << "\n";
    for (int j=0; j<=nLevels; ++j) {
    #pragma omp parallel for
        for (int k=0; k<nodesInLevel[j]; ++k) {
            for (int l=0; l<2; ++l) {
                tree[j][k]->Ufactor[l]  =   tree[j][k]->U[l];
                tree[j][k]->Vfactor[l]  =   tree[j][k]->V[l];
                // std::cout << "Level " << j << "; Node " << k << "; Matrix Size" << tree[j][k]->Ufactor[l].rows() << "\t" << tree[j][k]->Ufactor[l].cols() << "\n";
            }
        }
    }

    //  Factorize the leaf nodes
    #pragma omp parallel for
    for (int k=0; k<nodesInLevel[nLevels]; ++k) {
        factorize_Leaf(k);
    }
    //  Factorize the non-leaf nodes
    for (int j=nLevels-1; j>=0; --j) {
        #pragma omp parallel for
        for (int k=0; k<nodesInLevel[j]; ++k) {
            factorize_Non_Leaf(j, k);
        }
    }
    // std::cout << "\nEnd factorize...\n";
}

/**********************************************************************************************/
/*  PURPOSE OF EXISTENCE:   Solves for the  linear system Ax=b, where A is the kernel matrix. */
/**********************************************************************************************/

/************/
/*  INPUTS  */
/************/

/// b           -   Input matrix

/************/
/*  OUTPUT  */
/************/

/// x           -   Inverse of kernel matrix multiplied by input matrix

Eigen::MatrixXd HODLR_Tree::solve(Eigen::MatrixXd b) {
    // std::cout << "\nStart solve...\n";
    int start, size;
    Eigen::MatrixXd x   =   Eigen::MatrixXd::Zero(b.rows(),b.cols());
    int r   =   b.cols();
    // #pragma omp parallel for
    for (int k=0; k<nodesInLevel[nLevels]; ++k) {
        start   =   tree[nLevels][k]->nStart;
        size    =   tree[nLevels][k]->nSize;
        x.block(start, 0, size, r)  =   solve_Leaf(k, b.block(start, 0, size, r));
    }
    b=x;
    for (int j=nLevels-1; j>=0; --j) {
        // #pragma omp parallel for
        for (int k=0; k<nodesInLevel[j]; ++k) {
            start   =   tree[j][k]->nStart;
            size    =   tree[j][k]->nSize;
            x.block(start, 0, size, r)  =   solve_Non_Leaf(j, k, b.block(start, 0, size, r));
        }
        b=x;
    }
    // std::cout << "\nEnd solve...\n";
    return x;
} 
