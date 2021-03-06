// Generated by using Rcpp::compileAttributes() -> do not edit by hand
// Generator token: 10BE3573-1514-4C36-9D1C-5A225CD40393

#include "RxODE_types.h"
#include "../inst/include/RxODE_types.h"
#include <RcppArmadillo.h>
#include <Rcpp.h>

using namespace Rcpp;

// rxIs
bool rxIs(const RObject& obj, std::string cls);
RcppExport SEXP _RxODE_rxIs(SEXP objSEXP, SEXP clsSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< const RObject& >::type obj(objSEXP);
    Rcpp::traits::input_parameter< std::string >::type cls(clsSEXP);
    rcpp_result_gen = Rcpp::wrap(rxIs(obj, cls));
    return rcpp_result_gen;
END_RCPP
}
// rxModelVars_
List rxModelVars_(const RObject& obj);
RcppExport SEXP _RxODE_rxModelVars_(SEXP objSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< const RObject& >::type obj(objSEXP);
    rcpp_result_gen = Rcpp::wrap(rxModelVars_(obj));
    return rcpp_result_gen;
END_RCPP
}
// rxState
RObject rxState(const RObject& obj, RObject state);
RcppExport SEXP _RxODE_rxState(SEXP objSEXP, SEXP stateSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< const RObject& >::type obj(objSEXP);
    Rcpp::traits::input_parameter< RObject >::type state(stateSEXP);
    rcpp_result_gen = Rcpp::wrap(rxState(obj, state));
    return rcpp_result_gen;
END_RCPP
}
// rxParams_
CharacterVector rxParams_(const RObject& obj);
RcppExport SEXP _RxODE_rxParams_(SEXP objSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< const RObject& >::type obj(objSEXP);
    rcpp_result_gen = Rcpp::wrap(rxParams_(obj));
    return rcpp_result_gen;
END_RCPP
}
// rxDfdy
CharacterVector rxDfdy(const RObject& obj);
RcppExport SEXP _RxODE_rxDfdy(SEXP objSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< const RObject& >::type obj(objSEXP);
    rcpp_result_gen = Rcpp::wrap(rxDfdy(obj));
    return rcpp_result_gen;
END_RCPP
}
// rxLhs
CharacterVector rxLhs(const RObject& obj);
RcppExport SEXP _RxODE_rxLhs(SEXP objSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< const RObject& >::type obj(objSEXP);
    rcpp_result_gen = Rcpp::wrap(rxLhs(obj));
    return rcpp_result_gen;
END_RCPP
}
// rxInits
SEXP rxInits(const RObject& obj, RObject vec, Nullable<CharacterVector> req, double defaultValue, bool noerror, bool noini, bool rxLines);
RcppExport SEXP _RxODE_rxInits(SEXP objSEXP, SEXP vecSEXP, SEXP reqSEXP, SEXP defaultValueSEXP, SEXP noerrorSEXP, SEXP noiniSEXP, SEXP rxLinesSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< const RObject& >::type obj(objSEXP);
    Rcpp::traits::input_parameter< RObject >::type vec(vecSEXP);
    Rcpp::traits::input_parameter< Nullable<CharacterVector> >::type req(reqSEXP);
    Rcpp::traits::input_parameter< double >::type defaultValue(defaultValueSEXP);
    Rcpp::traits::input_parameter< bool >::type noerror(noerrorSEXP);
    Rcpp::traits::input_parameter< bool >::type noini(noiniSEXP);
    Rcpp::traits::input_parameter< bool >::type rxLines(rxLinesSEXP);
    rcpp_result_gen = Rcpp::wrap(rxInits(obj, vec, req, defaultValue, noerror, noini, rxLines));
    return rcpp_result_gen;
END_RCPP
}
// rxSetupIni
NumericVector rxSetupIni(const RObject& obj, RObject inits);
RcppExport SEXP _RxODE_rxSetupIni(SEXP objSEXP, SEXP initsSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< const RObject& >::type obj(objSEXP);
    Rcpp::traits::input_parameter< RObject >::type inits(initsSEXP);
    rcpp_result_gen = Rcpp::wrap(rxSetupIni(obj, inits));
    return rcpp_result_gen;
END_RCPP
}
// rxSetupScale
NumericVector rxSetupScale(const RObject& obj, RObject scale, Nullable<List> extraArgs);
RcppExport SEXP _RxODE_rxSetupScale(SEXP objSEXP, SEXP scaleSEXP, SEXP extraArgsSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< const RObject& >::type obj(objSEXP);
    Rcpp::traits::input_parameter< RObject >::type scale(scaleSEXP);
    Rcpp::traits::input_parameter< Nullable<List> >::type extraArgs(extraArgsSEXP);
    rcpp_result_gen = Rcpp::wrap(rxSetupScale(obj, scale, extraArgs));
    return rcpp_result_gen;
END_RCPP
}
// cvPost
RObject cvPost(double nu, RObject omega, int n, bool omegaIsChol, bool returnChol);
RcppExport SEXP _RxODE_cvPost(SEXP nuSEXP, SEXP omegaSEXP, SEXP nSEXP, SEXP omegaIsCholSEXP, SEXP returnCholSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< double >::type nu(nuSEXP);
    Rcpp::traits::input_parameter< RObject >::type omega(omegaSEXP);
    Rcpp::traits::input_parameter< int >::type n(nSEXP);
    Rcpp::traits::input_parameter< bool >::type omegaIsChol(omegaIsCholSEXP);
    Rcpp::traits::input_parameter< bool >::type returnChol(returnCholSEXP);
    rcpp_result_gen = Rcpp::wrap(cvPost(nu, omega, n, omegaIsChol, returnChol));
    return rcpp_result_gen;
END_RCPP
}
// rinvchisq
NumericVector rinvchisq(const int n, const double& nu, const double& scale);
RcppExport SEXP _RxODE_rinvchisq(SEXP nSEXP, SEXP nuSEXP, SEXP scaleSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< const int >::type n(nSEXP);
    Rcpp::traits::input_parameter< const double& >::type nu(nuSEXP);
    Rcpp::traits::input_parameter< const double& >::type scale(scaleSEXP);
    rcpp_result_gen = Rcpp::wrap(rinvchisq(n, nu, scale));
    return rcpp_result_gen;
END_RCPP
}
// rxSimThetaOmega
List rxSimThetaOmega(const Nullable<NumericVector>& params, const Nullable<NumericMatrix>& omega, const Nullable<NumericVector>& omegaDf, const bool& omegaIsChol, unsigned int nSub, const Nullable<NumericMatrix>& thetaMat, const Nullable<NumericVector>& thetaDf, const bool& thetaIsChol, unsigned int nStud, const Nullable<NumericMatrix> sigma, const Nullable<NumericVector>& sigmaDf, const bool& sigmaIsChol, int nCoresRV, unsigned int nObs, double dfSub, double dfObs, bool simSubjects);
RcppExport SEXP _RxODE_rxSimThetaOmega(SEXP paramsSEXP, SEXP omegaSEXP, SEXP omegaDfSEXP, SEXP omegaIsCholSEXP, SEXP nSubSEXP, SEXP thetaMatSEXP, SEXP thetaDfSEXP, SEXP thetaIsCholSEXP, SEXP nStudSEXP, SEXP sigmaSEXP, SEXP sigmaDfSEXP, SEXP sigmaIsCholSEXP, SEXP nCoresRVSEXP, SEXP nObsSEXP, SEXP dfSubSEXP, SEXP dfObsSEXP, SEXP simSubjectsSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< const Nullable<NumericVector>& >::type params(paramsSEXP);
    Rcpp::traits::input_parameter< const Nullable<NumericMatrix>& >::type omega(omegaSEXP);
    Rcpp::traits::input_parameter< const Nullable<NumericVector>& >::type omegaDf(omegaDfSEXP);
    Rcpp::traits::input_parameter< const bool& >::type omegaIsChol(omegaIsCholSEXP);
    Rcpp::traits::input_parameter< unsigned int >::type nSub(nSubSEXP);
    Rcpp::traits::input_parameter< const Nullable<NumericMatrix>& >::type thetaMat(thetaMatSEXP);
    Rcpp::traits::input_parameter< const Nullable<NumericVector>& >::type thetaDf(thetaDfSEXP);
    Rcpp::traits::input_parameter< const bool& >::type thetaIsChol(thetaIsCholSEXP);
    Rcpp::traits::input_parameter< unsigned int >::type nStud(nStudSEXP);
    Rcpp::traits::input_parameter< const Nullable<NumericMatrix> >::type sigma(sigmaSEXP);
    Rcpp::traits::input_parameter< const Nullable<NumericVector>& >::type sigmaDf(sigmaDfSEXP);
    Rcpp::traits::input_parameter< const bool& >::type sigmaIsChol(sigmaIsCholSEXP);
    Rcpp::traits::input_parameter< int >::type nCoresRV(nCoresRVSEXP);
    Rcpp::traits::input_parameter< unsigned int >::type nObs(nObsSEXP);
    Rcpp::traits::input_parameter< double >::type dfSub(dfSubSEXP);
    Rcpp::traits::input_parameter< double >::type dfObs(dfObsSEXP);
    Rcpp::traits::input_parameter< bool >::type simSubjects(simSubjectsSEXP);
    rcpp_result_gen = Rcpp::wrap(rxSimThetaOmega(params, omega, omegaDf, omegaIsChol, nSub, thetaMat, thetaDf, thetaIsChol, nStud, sigma, sigmaDf, sigmaIsChol, nCoresRV, nObs, dfSub, dfObs, simSubjects));
    return rcpp_result_gen;
END_RCPP
}
// rxSolveCsmall
SEXP rxSolveCsmall(const RObject& object, const Nullable<CharacterVector>& specParams, const Nullable<List>& extraArgs, const RObject& params, const RObject& events, const RObject& inits, const RObject& scale, const RObject& covs, const Nullable<List>& optsL);
RcppExport SEXP _RxODE_rxSolveCsmall(SEXP objectSEXP, SEXP specParamsSEXP, SEXP extraArgsSEXP, SEXP paramsSEXP, SEXP eventsSEXP, SEXP initsSEXP, SEXP scaleSEXP, SEXP covsSEXP, SEXP optsLSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< const RObject& >::type object(objectSEXP);
    Rcpp::traits::input_parameter< const Nullable<CharacterVector>& >::type specParams(specParamsSEXP);
    Rcpp::traits::input_parameter< const Nullable<List>& >::type extraArgs(extraArgsSEXP);
    Rcpp::traits::input_parameter< const RObject& >::type params(paramsSEXP);
    Rcpp::traits::input_parameter< const RObject& >::type events(eventsSEXP);
    Rcpp::traits::input_parameter< const RObject& >::type inits(initsSEXP);
    Rcpp::traits::input_parameter< const RObject& >::type scale(scaleSEXP);
    Rcpp::traits::input_parameter< const RObject& >::type covs(covsSEXP);
    Rcpp::traits::input_parameter< const Nullable<List>& >::type optsL(optsLSEXP);
    rcpp_result_gen = Rcpp::wrap(rxSolveCsmall(object, specParams, extraArgs, params, events, inits, scale, covs, optsL));
    return rcpp_result_gen;
END_RCPP
}
// rxSolveGet
RObject rxSolveGet(RObject obj, RObject arg, LogicalVector exact);
RcppExport SEXP _RxODE_rxSolveGet(SEXP objSEXP, SEXP argSEXP, SEXP exactSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< RObject >::type obj(objSEXP);
    Rcpp::traits::input_parameter< RObject >::type arg(argSEXP);
    Rcpp::traits::input_parameter< LogicalVector >::type exact(exactSEXP);
    rcpp_result_gen = Rcpp::wrap(rxSolveGet(obj, arg, exact));
    return rcpp_result_gen;
END_RCPP
}
// rxSolveUpdate
RObject rxSolveUpdate(RObject obj, RObject arg, RObject value);
RcppExport SEXP _RxODE_rxSolveUpdate(SEXP objSEXP, SEXP argSEXP, SEXP valueSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< RObject >::type obj(objSEXP);
    Rcpp::traits::input_parameter< RObject >::type arg(argSEXP);
    Rcpp::traits::input_parameter< RObject >::type value(valueSEXP);
    rcpp_result_gen = Rcpp::wrap(rxSolveUpdate(obj, arg, value));
    return rcpp_result_gen;
END_RCPP
}
// rxRmModelLib_
void rxRmModelLib_(std::string str);
RcppExport SEXP _RxODE_rxRmModelLib_(SEXP strSEXP) {
BEGIN_RCPP
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< std::string >::type str(strSEXP);
    rxRmModelLib_(str);
    return R_NilValue;
END_RCPP
}
// rxGetRxODE
RObject rxGetRxODE(RObject obj);
RcppExport SEXP _RxODE_rxGetRxODE(SEXP objSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< RObject >::type obj(objSEXP);
    rcpp_result_gen = Rcpp::wrap(rxGetRxODE(obj));
    return rcpp_result_gen;
END_RCPP
}
// rxIsCurrent
bool rxIsCurrent(RObject obj);
RcppExport SEXP _RxODE_rxIsCurrent(SEXP objSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< RObject >::type obj(objSEXP);
    rcpp_result_gen = Rcpp::wrap(rxIsCurrent(obj));
    return rcpp_result_gen;
END_RCPP
}
// rxAssignPtr
void rxAssignPtr(SEXP object);
RcppExport SEXP _RxODE_rxAssignPtr(SEXP objectSEXP) {
BEGIN_RCPP
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< SEXP >::type object(objectSEXP);
    rxAssignPtr(object);
    return R_NilValue;
END_RCPP
}
// rxCores
IntegerVector rxCores();
RcppExport SEXP _RxODE_rxCores() {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    rcpp_result_gen = Rcpp::wrap(rxCores());
    return rcpp_result_gen;
END_RCPP
}
// rxDll
std::string rxDll(RObject obj);
RcppExport SEXP _RxODE_rxDll(SEXP objSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< RObject >::type obj(objSEXP);
    rcpp_result_gen = Rcpp::wrap(rxDll(obj));
    return rcpp_result_gen;
END_RCPP
}
// rxC
CharacterVector rxC(RObject obj);
RcppExport SEXP _RxODE_rxC(SEXP objSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< RObject >::type obj(objSEXP);
    rcpp_result_gen = Rcpp::wrap(rxC(obj));
    return rcpp_result_gen;
END_RCPP
}
// rxIsLoaded
bool rxIsLoaded(RObject obj);
RcppExport SEXP _RxODE_rxIsLoaded(SEXP objSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< RObject >::type obj(objSEXP);
    rcpp_result_gen = Rcpp::wrap(rxIsLoaded(obj));
    return rcpp_result_gen;
END_RCPP
}
// rxDynLoad
bool rxDynLoad(RObject obj);
RcppExport SEXP _RxODE_rxDynLoad(SEXP objSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< RObject >::type obj(objSEXP);
    rcpp_result_gen = Rcpp::wrap(rxDynLoad(obj));
    return rcpp_result_gen;
END_RCPP
}
// rxDynUnload
bool rxDynUnload(RObject obj);
RcppExport SEXP _RxODE_rxDynUnload(SEXP objSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< RObject >::type obj(objSEXP);
    rcpp_result_gen = Rcpp::wrap(rxDynUnload(obj));
    return rcpp_result_gen;
END_RCPP
}
// rxDelete
bool rxDelete(RObject obj);
RcppExport SEXP _RxODE_rxDelete(SEXP objSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< RObject >::type obj(objSEXP);
    rcpp_result_gen = Rcpp::wrap(rxDelete(obj));
    return rcpp_result_gen;
END_RCPP
}
// rxInv
NumericVector rxInv(SEXP matrix);
RcppExport SEXP _RxODE_rxInv(SEXP matrixSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< SEXP >::type matrix(matrixSEXP);
    rcpp_result_gen = Rcpp::wrap(rxInv(matrix));
    return rcpp_result_gen;
END_RCPP
}
// rxToOmega
arma::mat rxToOmega(arma::mat cholMat);
RcppExport SEXP _RxODE_rxToOmega(SEXP cholMatSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< arma::mat >::type cholMat(cholMatSEXP);
    rcpp_result_gen = Rcpp::wrap(rxToOmega(cholMat));
    return rcpp_result_gen;
END_RCPP
}
// rxSymInvChol
RObject rxSymInvChol(RObject invObjOrMatrix, Nullable<NumericVector> theta, std::string type, int thetaNumber);
RcppExport SEXP _RxODE_rxSymInvChol(SEXP invObjOrMatrixSEXP, SEXP thetaSEXP, SEXP typeSEXP, SEXP thetaNumberSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< RObject >::type invObjOrMatrix(invObjOrMatrixSEXP);
    Rcpp::traits::input_parameter< Nullable<NumericVector> >::type theta(thetaSEXP);
    Rcpp::traits::input_parameter< std::string >::type type(typeSEXP);
    Rcpp::traits::input_parameter< int >::type thetaNumber(thetaNumberSEXP);
    rcpp_result_gen = Rcpp::wrap(rxSymInvChol(invObjOrMatrix, theta, type, thetaNumber));
    return rcpp_result_gen;
END_RCPP
}
// rxSymInvCholEnvCalculate
RObject rxSymInvCholEnvCalculate(List obj, std::string what, Nullable<NumericVector> theta);
RcppExport SEXP _RxODE_rxSymInvCholEnvCalculate(SEXP objSEXP, SEXP whatSEXP, SEXP thetaSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< List >::type obj(objSEXP);
    Rcpp::traits::input_parameter< std::string >::type what(whatSEXP);
    Rcpp::traits::input_parameter< Nullable<NumericVector> >::type theta(thetaSEXP);
    rcpp_result_gen = Rcpp::wrap(rxSymInvCholEnvCalculate(obj, what, theta));
    return rcpp_result_gen;
END_RCPP
}
// rxInvWishartVar
arma::mat rxInvWishartVar(arma::mat Omega, double nu);
RcppExport SEXP _RxODE_rxInvWishartVar(SEXP OmegaSEXP, SEXP nuSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< arma::mat >::type Omega(OmegaSEXP);
    Rcpp::traits::input_parameter< double >::type nu(nuSEXP);
    rcpp_result_gen = Rcpp::wrap(rxInvWishartVar(Omega, nu));
    return rcpp_result_gen;
END_RCPP
}
// RxODE_finalize_focei_omega
void RxODE_finalize_focei_omega(RObject rho);
RcppExport SEXP _RxODE_RxODE_finalize_focei_omega(SEXP rhoSEXP) {
BEGIN_RCPP
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< RObject >::type rho(rhoSEXP);
    RxODE_finalize_focei_omega(rho);
    return R_NilValue;
END_RCPP
}
// RxODE_finalize_log_det_OMGAinv_5
NumericVector RxODE_finalize_log_det_OMGAinv_5(SEXP rho);
RcppExport SEXP _RxODE_RxODE_finalize_log_det_OMGAinv_5(SEXP rhoSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< SEXP >::type rho(rhoSEXP);
    rcpp_result_gen = Rcpp::wrap(RxODE_finalize_log_det_OMGAinv_5(rho));
    return rcpp_result_gen;
END_RCPP
}
// rxCoutEcho
void rxCoutEcho(NumericVector number);
RcppExport SEXP _RxODE_rxCoutEcho(SEXP numberSEXP) {
BEGIN_RCPP
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< NumericVector >::type number(numberSEXP);
    rxCoutEcho(number);
    return R_NilValue;
END_RCPP
}
// removableDrive
bool removableDrive(std::string driveRoot);
RcppExport SEXP _RxODE_removableDrive(SEXP driveRootSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< std::string >::type driveRoot(driveRootSEXP);
    rcpp_result_gen = Rcpp::wrap(removableDrive(driveRoot));
    return rcpp_result_gen;
END_RCPP
}
