
#include "seqHMM.h"
using namespace Rcpp;

// Below is a simple example of exporting a C++ function to R. You can
// source this function into an R session using the Rcpp::sourceCpp 
// function (or via the Source button on the editor toolbar)

// For more on using Rcpp click the Help button on the editor toolbar
// install_github( "Rcpp11/attributes" ) ; require('attributes') 

// [[Rcpp::depends(RcppArmadillo)]]
// [[Rcpp::export]]

List EMx(NumericVector transitionMatrix, NumericVector emissionArray, NumericVector initialProbs,
  IntegerVector obsArray, int nSymbols, NumericMatrix coefs, NumericMatrix X_, IntegerVector numberOfStates,
  int itermax=100, double tol=1e-8, int trace=0) {  
  
  IntegerVector eDims = emissionArray.attr("dim"); //m,p,r
  IntegerVector oDims = obsArray.attr("dim"); //k,n,r
  
  arma::vec init(initialProbs.begin(), eDims[0], true);
  arma::mat transition(transitionMatrix.begin(), eDims[0], eDims[0], true);
  arma::mat emission(emissionArray.begin(), eDims[0], eDims[1],true);
  arma::Mat<int> obs(obsArray.begin(), oDims[0], oDims[1],false);
  
  int q = coefs.nrow();
  arma::mat coef(coefs.begin(),q,coefs.ncol());
  coef.col(0).zeros();
  arma::mat X(X_.begin(),oDims[0],q);
  arma::mat lweights = exp(X*coef).t();
  if(!lweights.is_finite()){
    return List::create(Named("error") = "error");
  }
  lweights.each_row() /= sum(lweights,0);
  lweights = log(lweights); 
  transition = log(transition); 
  emission = log(emission);
  init = log(init); 

  arma::mat initk(eDims[0],oDims[0]);
  for(int k = 0; k < oDims[0]; k++){    
    initk.col(k) = init + reparma(lweights.col(k),numberOfStates);
  }
  
  arma::cube alpha(eDims[0],oDims[1],oDims[0]); //m,n,k
  arma::cube beta(eDims[0],oDims[1],oDims[0]); //m,n,k
  
  internalForwardx(transition, emission, initk, obs, alpha);
  internalBackward(transition, emission, obs, beta);
  
  arma::vec ll(oDims[0]);
  
  double tmp = 0.0;
  double sumtmp = 0.0;
  double neginf = -arma::math::inf();
  
  
  
  for(int k=0;k<oDims[0];k++){    
    tmp =neginf;
    for(int i = 0; i < eDims[0]; i++){
      if(alpha(i,oDims[1]-1,k)>neginf){
        tmp = logSumExp(alpha(i,oDims[1]-1,k),tmp); 
      }
    }
    ll(k) = tmp;
   
  }
  
  double sumlogLik = sum(ll);
  if(trace>0){
    Rcout<<"Log-likelihood of initial model: "<< sumlogLik<<std::endl;
  }
  //  
  //  //EM-algorithm begins
  //  
  double change = tol+1.0;
  int iter = 0;
  arma::mat ksii(eDims[0],eDims[0]);
  arma::mat gamma(eDims[0],eDims[1]-1);
  arma::vec delta(eDims[0]);
  
  
  IntegerVector cumsumstate = cumsum(numberOfStates);
 
 
  while((change>tol) & (iter<itermax)){   
    iter++;
    gamma.zeros();
    ksii.zeros();
    delta.zeros();        

    for(int k = 0; k < oDims[0]; k++){
      
     
      delta += exp(alpha.slice(k).col(0) + beta.slice(k).col(0) - ll(k));
      
      for(int i = 0; i < eDims[0]; i++){
        for(int j = 0; j < eDims[0]; j++){
          sumtmp = neginf;
          for(int t=0; t < (oDims[1]-1); t++){
            tmp = alpha(i,t,k) + transition(i,j) + beta(j,t+1,k) + emission(j,obs(k,t+1));
            if(tmp>neginf){
              sumtmp = logSumExp(sumtmp,tmp);
            }
          }
          ksii(i,j) += exp(sumtmp-ll(k));
        }
      }
      
      
        for(int i = 0; i<eDims[0]; i++){
          for(int l = 0; l<nSymbols; l++){
            sumtmp = neginf;
            for(int t=0; t<oDims[1];t++){
              if(l == (obs(k,t))){
                tmp = alpha(i,t,k) + beta(i,t,k);
                if(tmp>neginf){
                  sumtmp = logSumExp(sumtmp,tmp);
                }
              }              
            }
            gamma(i,l) += exp(sumtmp-ll(k));
          }
        }
           
    }
    
    ksii.each_col() /= sum(ksii,1);
    transition = log(ksii);
    gamma.each_col() /= sum(gamma,1);    
    emission.cols(0,nSymbols-1) = log(gamma);
    
    for(int i=0; i < numberOfStates.size(); i++){
      delta.subvec(cumsumstate(i)-numberOfStates(i),cumsumstate(i)-1) /= 
        arma::as_scalar(arma::accu(delta.subvec(cumsumstate(i)-numberOfStates(i),cumsumstate(i)-1)));
    }   
    init = log(delta);
    
    
    optCoef(alpha.tube(0,0,alpha.n_rows-1,0),beta.tube(0,0,alpha.n_rows-1,0),ll,coef,X,lweights, cumsumstate,numberOfStates);
    
  
    
    for(int k = 0; k < oDims[0]; k++){    
      initk.col(k) = init + reparma(lweights.col(k),numberOfStates);
    }
    
    internalForwardx(transition, emission, initk, obs, alpha);
    internalBackward(transition, emission, obs, beta);
    
    for(int k=0;k<oDims[0];k++){
      tmp =neginf;
      for(int i = 0; i < eDims[0]; i++){
        if(alpha(i,oDims[1]-1,k)>neginf){
          tmp = logSumExp(alpha(i,oDims[1]-1,k),tmp); 
        }
      }
      ll(k) = tmp;
    }
    
    
    tmp = sum(ll);
    change = (tmp - sumlogLik)/(abs(sumlogLik)+0.1);
    sumlogLik = tmp;
    if(trace>1){
      Rcout<<"iter: "<< iter;
      Rcout<<" logLik: "<< sumlogLik;
      Rcout<<" relative change: "<<change<<std::endl;
    }
    
  }
  if(trace>0){
    if(iter==itermax){
      Rcpp::Rcout<<"EM algorithm stopped after reaching the maximum number of "<<iter<<" iterations."<<std::endl;     
    } else{
      Rcpp::Rcout<<"EM algorithm stopped after reaching the relative change of "<<change;
      Rcpp::Rcout<<" after "<<iter<<" iterations."<<std::endl;
    }
    Rcpp::Rcout<<"Final log-likelihood: "<< sumlogLik<<std::endl;
  }
  return List::create(Named("beta") = wrap(coef), Named("initialProbs") = wrap(exp(init)), Named("transitionMatrix") = wrap(exp(transition)),
    Named("emissionArray") = wrap(exp(emission)),Named("logLik") = sumlogLik,Named("iterations")=iter,Named("change")=change);
}