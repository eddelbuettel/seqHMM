// Internal forward algorithms for HMMs and MHMMs

#include "seqHMM.h"
void internalForward(const arma::mat& transition, const arma::cube& emission, const arma::vec& init,
  const arma::icube& obs, arma::cube& alpha, arma::mat& scales, int threads) {

#pragma omp parallel for if(obs.n_slices >= threads) schedule(static) num_threads(threads) \
  default(none) shared(alpha, scales, obs, init, emission, transition)
    for (int k = 0; k < obs.n_slices; k++) {
      alpha.slice(k).col(0) = init;
      for (unsigned int r = 0; r < obs.n_rows; r++) {
        alpha.slice(k).col(0) %= emission.slice(r).col(obs(r, 0, k));
      }
      scales(0, k) = sum(alpha.slice(k).col(0));
      alpha.slice(k).col(0) /= scales(0, k);
      for (unsigned int t = 1; t < obs.n_cols; t++) {
        alpha.slice(k).col(t) = transition.t() * alpha.slice(k).col(t - 1);
        for (unsigned int r = 0; r < obs.n_rows; r++) {
          alpha.slice(k).col(t) %= emission.slice(r).col(obs(r, t, k));
        }
        scales(t, k) = sum(alpha.slice(k).col(t));
        alpha.slice(k).col(t) /= scales(t, k);
      }
    }
}

void internalForwardx(const arma::sp_mat& transition_t, const arma::cube& emission,
  const arma::mat& init, const arma::icube& obs, arma::cube& alpha, arma::mat& scales,
  int threads) {


#pragma omp parallel for if(obs.n_slices >= threads) schedule(static) num_threads(threads) \
  default(none) shared(alpha, scales, obs, init, emission, transition_t)
    for (int k = 0; k < obs.n_slices; k++) {

      alpha.slice(k).col(0) = init.col(k);
      for (unsigned int r = 0; r < obs.n_rows; r++) {
        alpha.slice(k).col(0) %= emission.slice(r).col(obs(r, 0, k));
      }
      scales(0, k) = sum(alpha.slice(k).col(0));
      alpha.slice(k).col(0) /= scales(0, k);
      for (unsigned int t = 1; t < obs.n_cols; t++) {
        alpha.slice(k).col(t) = transition_t * alpha.slice(k).col(t - 1);
        for (unsigned int r = 0; r < obs.n_rows; r++) {
          alpha.slice(k).col(t) %= emission.slice(r).col(obs(r, t, k));
        }
        scales(t, k) = sum(alpha.slice(k).col(t));
        alpha.slice(k).col(t) /= scales(t, k);
      }
    }
}
