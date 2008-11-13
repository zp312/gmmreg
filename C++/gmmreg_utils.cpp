/*=========================================================================
$Author: bjian $
$Date: 2008/06/05 17:06:23 $
$Revision: 1.1 $
=========================================================================*/

/** 
 * \file gmmreg_utils.cpp
 * \brief  The definition of supporting functions
 */


#include <vnl/vnl_vector.h>
#include <vnl/vnl_matrix.h>
#include <vcl_iostream.h>

#define SQR(X)  ((X)*(X))
#include <cmath>
#include <vector>
#include <iostream>
#include <fstream>
//#include <set>
/* #include <stdio.h> */
#include "gmmreg_utils.h"

/*
 *  Note: The input point set containing 'n' points in 'd'-dimensional
 *  space should be arranged in the memory such that the j-th coordinate of i-th point
 *  is at location  (i*d + j), i.e., the input matrix in MATLAB should
 *  be of size (d, n) since MATLAB is column-major while the input array
 *  in NumPy should be of size (n, d) since NumPy is row-major
 */

double GaussTransform(const double* A, const double* B, int m, int n, int dim, double scale)
{
    int i,j,d; 
    int id, jd;
    double dist_ij, cross_term = 0;
    double cost_ij;
    scale = SQR(scale);
    for (i=0;i<m;++i)
    {
        for (j=0;j<n;++j)
        {
            dist_ij = 0;
            for (d=0;d<dim;++d)
            {
                id = i*dim + d;
                jd = j*dim + d;
                dist_ij +=  SQR( A[id] - B[jd]);
            }
            cost_ij = exp(-dist_ij/scale);
            cross_term += cost_ij;
        }
        /* printf("cross_term = %.3f\n", cross_term); */
    }
    return cross_term/(m*n);
}



void GaussianAffinityMatrix(const double* A, const double* B, int m, int n, int dim, double scale, double* dist)
{
    int i,j,k,d; 
    int id, jd;
    double dist_ij;

    scale = -2*SQR(scale);
    for (i=0,k=0;i<m;++i)
    {
        for (j=0;j<n;++j,++k)
        {
            dist_ij = 0;
            for (d=0;d<dim;++d)
            {
                id = i*dim + d;
                jd = j*dim + d;
                dist_ij +=  SQR( A[id] - B[jd]);
            }
            dist[k] = exp(dist_ij/scale);
        }
    }
}


double GaussTransform(const double* A, const double* B, int m, int n, int dim, double scale, double* grad)
{
    int i,j,d; 
    int id, jd;
    double dist_ij, cross_term = 0;
    double cost_ij;
    for (i=0;i<m*dim;++i) grad[i] = 0;

    scale = SQR(scale);
    for (i=0;i<m;++i)
    {
        for (j=0;j<n;++j)
        {
            dist_ij = 0;
            for (d=0;d<dim;++d)
            {
                id = i*dim + d;
                jd = j*dim + d;
                dist_ij +=  SQR( A[id] - B[jd]);
            }
            cost_ij = exp(-dist_ij/scale);
            for (d=0;d<dim;++d){
                id = i*dim + d;
                jd = j*dim + d;
                grad[id] -= cost_ij*2*(A[id] - B[jd]);
            }
            cross_term += cost_ij;
        }
        /* printf("cross_term = %.3f\n", cross_term); */
    }
    scale *= m*n;
    for (i=0;i<m*dim;++i) {
        grad[i] /= scale;
    }
    return cross_term/(m*n);
}


/*  This version is too slow
double GaussTransform(const vnl_matrix<double>& A, const vnl_matrix<double>& B, double scale, vnl_matrix<double>& gradient)
{
     int m,n,d;
     vnl_vector<double> g_i, v_ij;
     double dist_ij, cost_ij, cost;
     m = A.rows(); n = B.rows(); d = A.cols();  // assert A.cols() == B.cols()
     cost = 0;
     for(int i=0;i<m;++i)
     {
         g_i = gradient.get_row(i)*m*n*scale*scale;
         for (int j=0;j<n;++j)
         {
             v_ij = A.get_row(i) - B.get_row(j);
             dist_ij = v_ij.squared_magnitude();
             cost_ij = exp(-dist_ij/(scale*scale));
             g_i -= cost_ij*2*v_ij;
             cost += cost_ij;
         }
         gradient.set_row(i, g_i);   		
     }
     cost /= m*n*1.0;
     gradient /= m*n*scale*scale;
     return cost;
}
*/

double GaussTransform(const vnl_matrix<double>& A, const vnl_matrix<double>& B, double scale)
{
     int m,n,d;
     double cost;
     m = A.rows(); n = B.rows(); d = A.cols();  // assert A.cols() == B.cols()
     cost =  GaussTransform(A.data_block(), B.data_block(), m,n,d,scale);
     return cost;
}

double GaussTransform(const vnl_matrix<double>& A, const vnl_matrix<double>& B, double scale, vnl_matrix<double>& gradient)
{
     int m,n,d;
     double cost;
     m = A.rows(); n = B.rows(); d = A.cols();  // assert A.cols() == B.cols()
     cost =  GaussTransform(A.data_block(), B.data_block(), m,n,d,scale, gradient.data_block());
     return cost;
}

/*
double GaussTransform(const vnl_matrix<double>& A, const vnl_matrix<double>& B, double scale, double* gradient)
{
     int m,n,d;
     vnl_vector<double> g_i, v_ij;
     double cost;
     m = A.rows(); n = B.rows(); d = A.cols();  // assert A.cols() == B.cols()
     cost =  GaussTransform(A.data_block(), B.data_block(), m,n,d,scale, gradient);
     return cost;
}
*/


// todo: add one more version when the model is same as ctrl_pts
// reference:  Landmark-based Image Analysis, Karl Rohr, p195 
void ComputeTPSKernel(const vnl_matrix<double>& model, const vnl_matrix<double>& ctrl_pts, vnl_matrix<double>& U, vnl_matrix<double>& K)
{
      int m,n,d;
      m = model.rows();
      n = ctrl_pts.rows();
      d = ctrl_pts.cols();
      //asssert(model.cols()==d==(2|3));

      K.set_size(n,n); 
      U.set_size(m,n); 

      vnl_vector<double> v_ij;
      for (int i=0;i<m;++i)
      {
          for (int j=0;j<n;++j)
          {
              v_ij = model.get_row(i) - ctrl_pts.get_row(j);
              if (d==2)
              {
                  double r = v_ij.squared_magnitude();
                  if (r>0)
                    U(i,j) = r*log(r)/2;
                  else
                    U(i,j) = 0;
              }
              else if (d==3)
              {
                  double r = v_ij.two_norm();
                  U(i,j) = -r;
              }
          }
      }
      for (int i=0;i<n;++i)
      {
          for (int j=0;j<n;++j)
          {
              v_ij = ctrl_pts.get_row(i) - ctrl_pts.get_row(j);
              if (d==2)
              {
                  double r = v_ij.squared_magnitude();
                  if (r>0)
                  K(i,j) = r*log(r)/2;
              }
              else if (d==3)
              {
                  double r = v_ij.two_norm();
                  K(i,j) = -r;
              }
          }
      }
}

/*
Matlab code in cpd_G.m:
k=-2*beta^2;
[n, d]=size(x); [m, d]=size(y);

G=repmat(x,[1 1 m])-permute(repmat(y,[1 1 n]),[3 2 1]);
G=squeeze(sum(G.^2,2));
G=G/k;
G=exp(G);
*/
void ComputeGaussianKernel(const vnl_matrix<double>& model, const vnl_matrix<double>& ctrl_pts, vnl_matrix<double>& G, vnl_matrix<double>& K, double lambda)
{
      int m,n,d;
      m = model.rows();
      n = ctrl_pts.rows();
      d = ctrl_pts.cols();
      //asssert(model.cols()==d);
      //assert(lambda>0);	

      G.set_size(m,n); 
      GaussianAffinityMatrix(model.data_block(), ctrl_pts.data_block(), m,n,d,lambda,G.data_block());

/*
      //vnl_vector<double> v_ij;
      double r, den = -2*lambda*lambda;
      for (int i=0;i<m;++i)
      {
          for (int j=0;j<n;++j)
          {
             r = 0;
             for (int t=0;t<d;++t)
             {
                r += (model(i,t) - ctrl_pts(j,t))*(model(i,t) - ctrl_pts(j,t));
             }
              //vcl_cout << r/den << vcl_endl;
              G(i,j) = exp(r/den);
          }
      }
*/
      if (model==ctrl_pts)
      {
          K = G;
      }
      else{
      K.set_size(n,n); 
      GaussianAffinityMatrix(ctrl_pts.data_block(), ctrl_pts.data_block(), n,n,d,lambda,K.data_block());
      }
/*	  for (int i=0;i<n;++i)
      {
          for (int j=0;j<n;++j)
          {
             r = 0;
             for (int t=0;t<d;++t)
             {
                r += (ctrl_pts(i,t) - ctrl_pts(j,t))*(ctrl_pts(i,t) - ctrl_pts(j,t));
             }
              K(i,j) = exp(r/den);
          }
      }
*/

}

void ComputeSquaredDistanceMatrix(const vnl_matrix<double>& A, const vnl_matrix<double>& B, vnl_matrix<double>& D)
{
      int m = A.rows();
      int n = B.rows();
      //asssert(A.cols()==B.cols());
      D.set_size(m,n);
      vnl_vector<double> v_ij;
      for (int i=0;i<m;++i)
      {
          for (int j=0;j<n;++j)
          {
              v_ij = A.get_row(i) - B.get_row(j);
              D(i,j) = v_ij.squared_magnitude();
          }
      }
}

void parse_tokens(char* str, const char delims[], std::vector<double>& v_tokens)
{
    char* pch = strtok (str,delims);
    while (pch != NULL)
    {
        v_tokens.push_back(atof(pch));
        pch = strtok (NULL, delims);
    }
}

void parse_tokens(char* str, const char delims[], std::vector<int>& v_tokens)
{
    char* pch = strtok (str,delims);
    while (pch != NULL)
    {
        v_tokens.push_back(atoi(pch));
        pch = strtok (NULL, delims);
    }
}


int select_points(const vnl_matrix<double>&pts, const std::vector<int>&index, vnl_matrix<double>& selected)
{
    int n = index.size();
    int d = pts.cols();
    selected.set_size(n,d);
    for (int i=0;i<n;++i)
        selected.update(pts.extract(1,d,index[i]),i);
    return n;
}

void pick_indices(const vnl_matrix<double>&dist, std::vector<int>&row_index, std::vector<int>&col_index, const double threshold)
{
    int m = dist.rows();
    int n = dist.cols();
    vnl_vector<int> row_flag, col_flag;
    col_flag.set_size(n); col_flag.fill(0);
    row_flag.set_size(n); row_flag.fill(0);
    for (int i=0;i<m;++i)
    {
        for (int j=0;j<n;++j)
        {
            //std::cout << "i " << i << " j "<< j << " dist "<<dist(i,j) << std::endl; 
            if (dist(i,j)<threshold){
                if (row_flag[i]==0)
                {
                    row_index.push_back(i);
                    row_flag[i] = 1;
                }
                //std::cout << "add row " << i << std::endl;
                if (col_flag[j]==0){
                    col_index.push_back(j);
                    col_flag[j] = 1;
                    //std::cout << "add col " << j << std::endl;
                }
            }
        }
    }
}

void find_working_pair(const vnl_matrix<double>&M, const vnl_matrix<double>&S,
                       const vnl_matrix<double>&Transformed_M, const double threshold,
                       vnl_matrix<double>&working_M, vnl_matrix<double>&working_S)
{
    vnl_matrix<double> dist;
    ComputeSquaredDistanceMatrix(Transformed_M, S, dist);
    std::vector<int> row_index, col_index;
    pick_indices(dist, row_index, col_index, threshold);
    std::cout << "selected rows:" << row_index.size() << std::endl;
    std::cout << "selected cols:" << col_index.size() << std::endl;
    select_points(M, row_index, working_M);
    //std::cout << working_M.rows() << std::endl;
    select_points(S, col_index, working_S);
    //std::cout << working_S.rows() << std::endl;
}


int get_config_fullpath(const char* input_config,char* f_config)
{
#ifdef WIN32
    char* lpPart[BUFSIZE]={NULL};
    int retval = GetFullPathName(input_config,
                 BUFSIZE, f_config,
                 lpPart);

    if (retval == 0) 
    {
        // Handle an error condition.
        printf ("GetFullPathName failed (%d)\n", GetLastError());
        return -1;
    }
    else {
        printf("The full path name is:  %s\n", f_config);

    }
#else
    strcpy(f_config,input_config);
#endif
    return 0;
}


void save_matrix( const char * filename, const vnl_matrix<double>& x)
{
    if (strlen(filename)>0) 
    {
         std::ofstream outfile(filename,std::ios_base::out);
         x.print(outfile);
    }
}


void normalize(vnl_matrix<double>& x, vnl_vector<double>& centroid, double& scale)
{
    int n = x.rows();
    int d = x.cols();
    centroid.set_size(d);

    vnl_vector<double> col;	
    for (int i=0;i<d;++i){
        col = x.get_column(i);
        centroid(i) = col.mean();
    }
    for (int i=0;i<n;++i){
        x.set_row(i, x.get_row(i)-centroid);
    }
    scale = x.frobenius_norm()/sqrt(double(n));
    x = x/scale;

}

void denormalize(vnl_matrix<double>& x, const vnl_vector<double>& centroid, const double scale)
{
    int n = x.rows();
    int d = x.cols();
    for (int i=0;i<n;++i){
        x.set_row(i, x.get_row(i)*scale+centroid);
    }
}

