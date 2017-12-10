// [[Rcpp::depends(RcppArmadillo)]]
#include <RcppArmadillo.h>
extern "C"{
#include "solve.h"
  rx_solve *getRxSolve_(SEXP ptr);
  void rxSolveDataFree(SEXP ptr);
}

using namespace Rcpp;
using namespace arma;

int rxcEvid = -1;
int rxcTime = -1;
int rxcAmt  = -1;
int rxcId   = -1;
int rxcDv   = -1;
int rxcLen  = -1;
bool resetCache = true;
bool rxHasEventNames(CharacterVector &nm){
  int len = nm.size();
  bool reset  = resetCache;
  if (reset || len != rxcLen){
    reset   = resetCache;
    rxcEvid = -1;
    rxcTime = -1;
    rxcAmt  = -1;
    rxcId   = -1;
    rxcDv   = -1;
    rxcLen  = len;
    for (int i = 0; i < len; i++){
      if (as<std::string>(nm[i]) == "evid" || as<std::string>(nm[i]) == "EVID" || as<std::string>(nm[i]) == "Evid"){
        rxcEvid = i;
      } else if (as<std::string>(nm[i]) == "time" || as<std::string>(nm[i]) == "TIME" || as<std::string>(nm[i]) == "Time"){
        rxcTime = i;
      } else if (as<std::string>(nm[i]) == "amt" || as<std::string>(nm[i]) == "AMT" || as<std::string>(nm[i]) == "Amt"){
        rxcAmt = i;
      } else if (as<std::string>(nm[i]) == "id" || as<std::string>(nm[i]) == "ID" || as<std::string>(nm[i]) == "Id"){
        rxcId = i;
      } else if (as<std::string>(nm[i]) == "dv" || as<std::string>(nm[i]) == "DV" || as<std::string>(nm[i]) == "Dv"){
        rxcDv = i;
      }
    }
  }
  resetCache = true;
  if (rxcEvid >= 0 && rxcTime >= 0 && rxcAmt >= 0){
    return true;
  } else {
    return false;
  }
}

//' Check the type of an object using Rcpp
//'
//' @param obj Object to check
//' @param cls Type of class.  Only s3 classes and primitive classes are checked.
//'    For matrix types they are distinguished as \code{numeric.matrix}, \code{integer.matrix},
//'    \code{logical.matrix}, and \code{character.matrix} as well as the traditional \code{matrix}
//'    class. Additionally checks for \code{event.data.frame} which is an \code{data.frame} object
//'    with \code{time},  \code{evid} and \code{amt}. (UPPER, lower or Title cases accepted)
//'
//' @return A boolean indicating if the object is a member of the class.
//' @keywords internal
//' @author Matthew L. Fidler
//' @export
// [[Rcpp::export]]
bool rxIs(const RObject &obj, std::string cls){
  if (cls == "rx.event"){
    return (rxIs(obj, "EventTable") || rxIs(obj, "event.data.frame") || rxIs(obj, "event.matrix"));
  } else if (cls == "event.data.frame"){
    if (rxIs(obj, "data.frame")){
      CharacterVector cv =as<CharacterVector>((as<DataFrame>(obj)).names());
      return rxHasEventNames(cv);
    } else {
      return false;
    }
  } else if (cls == "event.matrix"){
    if (rxIs(obj,"numeric.matrix") && obj.hasAttribute("dimnames")){
      List dn = as<List>(obj.attr("dimnames"));
      if (dn.size() == 2){
	CharacterVector cv = as<CharacterVector>(dn[1]);
        return rxHasEventNames(cv);
      } else {
	return false; // nocov
      }
    } else {
      return false;
    }
  } else if (obj.isObject()){
    CharacterVector classattr = obj.attr("class");
    for (int i = 0; i < classattr.size(); i++){
      if (as<std::string>(classattr[i]) == cls){
        return true;
      }
    }
  } else {
    int type = obj.sexp_type();
    bool hasDim = obj.hasAttribute("dim");
    if (type == REALSXP){
      if (hasDim){
	if (cls == "numeric.matrix" || cls == "matrix"){
	  return true;
	} else {
	  return false;
	}
      } else {
	if (cls == "numeric")
          return true;
        else 
          return false;
      }
    }
    if (type == INTSXP){
      if (hasDim){
	if (cls == "integer.matrix" || cls == "matrix"){
          return true;
        } else {
          return false;
        }
      } else {
	if (cls == "integer")
          return true;
        else
          return false;
      }
    }
    if (type == LGLSXP){
      if (hasDim){
        if (cls == "logical.matrix" || cls == "matrix"){
          return true;
        } else {
          return false;
        }
      } else {
	if (cls == "logical")
          return true;
        else
          return false;
      }
    }
    if (type == STRSXP){
      if (hasDim){
	if (cls == "character.matrix" || cls == "matrix"){
          return true;
        } else {
          return false;
        }
      } else {
	if (cls == "character")
          return true;
        else
          return false;
      }
    }
    if (type == VECSXP){
      if (cls == "list"){
        return true;
      } else {
        return false;
      }
    }
  }
  return false;
}

RObject rxSimSigma(const RObject &sigma,
		   const RObject &df,
		   const int &ncores,
		   const bool &isChol,
		   int &nObs){
  if (rxIs(sigma, "numeric.matrix")){
    // FIXME more distributions
    NumericMatrix sigmaM(sigma);
    if (sigmaM.nrow() != sigmaM.ncol()){
      stop("The sigma matrix must be a square matrix.");
    }
    if (!sigmaM.hasAttribute("dimnames")){
      stop("The sigma matrix must have named dimensions.");
    }
    List dimnames = sigmaM.attr("dimnames");
    StringVector simNames = as<StringVector>(dimnames[1]);
    Environment base("package:base");
    Function loadNamespace=base["loadNamespace"];
    Environment mvnfast = loadNamespace("mvnfast");
    NumericMatrix simMat(nObs,sigmaM.ncol());
    NumericVector m(sigmaM.ncol());
    // I'm unsure if this for loop is necessary.
    for (int i = 0; i < m.size(); i++){
      m[i] = 0;
    }
    // Ncores = 1?  Should it be parallelized when it can be...?
    // Note that if so, the number of cores also affects the output.
    if (df.isNULL()){
      Function rmvn = as<Function>(mvnfast["rmvn"]);
      rmvn(_["n"]=nObs, _["mu"]=m, _["sigma"]=sigmaM, _["ncores"]=ncores, _["isChol"]=isChol, _["A"] = simMat); // simMat is updated with the random deviates
    } else {
      double df2 = as<double>(df);
      if (R_FINITE(df2)){
	Function rmvt = as<Function>(mvnfast["rmvt"]);
        rmvt(_["n"]=nObs, _["mu"]=m, _["sigma"]=sigmaM, _["df"] = df, _["ncores"]=ncores, _["isChol"]=isChol, _["A"] = simMat);
      } else {
	Function rmvn = as<Function>(mvnfast["rmvn"]);
        rmvn(_["n"]=nObs, _["mu"]=m, _["sigma"]=sigmaM, _["ncores"]=ncores, _["isChol"]=isChol, _["A"] = simMat);
      }
    }
    simMat.attr("dimnames") = List::create(R_NilValue, simNames);
    return wrap(simMat);
  } else {
    return R_NilValue;
  }
}

// [[Rcpp::export]]
List rxDataSetup(const RObject &ro,
		 const RObject &covNames = R_NilValue,
		 const RObject &sigma = R_NilValue,
		 const RObject &df = R_NilValue,
		 const int &ncoresRV = 1,
		 const bool &isChol = false,
		 const StringVector &amountUnits = NA_STRING,
		 const StringVector &timeUnits = "hours"){
  // Purpose: get positions of each id and the length of each id's observations
  // Separate out dose vectors and observation vectors
  if (rxIs(ro,"EventTable")){
    List et = List(ro);
    Function f = et["get.EventTable"];
    DataFrame dataf = f();
    f = et["get.units"];
    RObject unitsRO = f();
    CharacterVector units;
    int i, n;
    if (rxIs(unitsRO, "character")){
      units = as<CharacterVector>(unitsRO);
      n=units.size();
      for (i =0; i<n; i++){
	if (units[i] == "NA"){
	  units[i] = NA_STRING;
	}
      }
    } else {
      units = CharacterVector::create(_["dosing"]=NA_STRING,
				      _["time"]=NA_STRING);
    }
    // {
    //   units = StringVector(unitsRO);
    //   if (units[0] == "NA"){
    // 	units[0] = NA_STRING;
    //   }
    //   if (units[1] == "NA"){
    // 	units[1] = NA_STRING;
    //   }
    // } else {
    //   // Otherwise this is likely 2 NAs.
    //   units[0] = NA_STRING;
    //   units[1] = NA_STRING;
    //   StringVector units2(2);
    //   units2[0] = "dosing";
    //   units2[1] = "time";
    //   units.names() = units;
    // }
    CharacterVector amt = (units["dosing"] == NA_STRING) ? StringVector::create(NA_STRING) : as<StringVector>(units["dosing"]);
    CharacterVector time = (units["time"] == NA_STRING) ? StringVector::create(NA_STRING) : as<StringVector>(units["time"]);
    return rxDataSetup(dataf, covNames, sigma, df, ncoresRV, isChol, amt, time);
  } else if (rxIs(ro,"event.data.frame")||
      rxIs(ro,"event.matrix")){
    DataFrame dataf = as<DataFrame>(ro);
    int nSub = 0, nObs = 0, nDoses = 0, i = 0, j = 0, k=0;
    // Since the event data frame can be "wild", these need to be
    // converted to integers.
    IntegerVector evid  = as<IntegerVector>(dataf[rxcEvid]);
    bool missingId = false;
    IntegerVector id(evid.size());
    if (rxcId > -1){
      id    = as<IntegerVector>(dataf[rxcId]);
    } else {
      for (i = 0; i < evid.size(); i++){
	id[i]=1;
      }
      missingId=true;
    }
    bool missingDv = false;
    NumericVector dv(evid.size());
    if (rxcDv > -1){
      dv = as<NumericVector>(dataf[rxcDv]);
    } else {
      for (i = 0; i < evid.size(); i++){
	dv[i] = NA_REAL;
      }
      missingDv = true;
    }
    NumericVector time0 = dataf[rxcTime];
    NumericVector amt   = dataf[rxcAmt];
    int ids = id.size();
    int lastId = id[0]-1;
    // Get the number of subjects
    // Get the number of observations
    // Get the number of doses
    for (i = 0; i < ids; i++){
      if (lastId != id[i]){
        nSub++;
        lastId=id[i];
      }
      if (evid[i]){
        nDoses++;
      } else {
        nObs++;
      }
    }
    // Now create data frames of observations and events
    NumericVector newDv(nObs);
    NumericVector newTimeO(nObs);
    int nCovs = 0;
    StringVector covN, simN;
    bool dataCov = false;
    DataFrame covDf;
    NumericMatrix simMat;
    StringVector simNames;
    bool simVals = false;
    RObject tmp_ro = rxSimSigma(sigma, df, ncoresRV, isChol, nObs);
    if (!tmp_ro.isNULL()){
      simMat = NumericMatrix(tmp_ro);
      List dimnames = simMat.attr("dimnames");
      simNames = StringVector(dimnames[1]);
      simVals = true;
    }
    int nCovObs = 0;
    if (rxIs(covNames, "character")){
      covN = StringVector(covNames);
      nCovObs = covN.size();
      if (simVals){
	nCovs= nCovObs + simNames.size();
      } else {
        nCovs = nCovObs;
      }
    } else if (rxIs(covNames, "data.frame") || rxIs(covNames,"numeric.matrix")){
      covDf = as<DataFrame>(covNames);
      covN = StringVector(covDf.names());
      nCovObs = covN.size();
      if (simVals){
	nCovs = nCovObs + simNames.size();
      } else {
	nCovs = nCovObs;
      }
      dataCov = true;
    } else if (simVals) {
      nCovs= simNames.size();
    }
    
    // Rprintf("nObs: %d; nCovs: %d\n", nObs, nCovs);
    NumericVector newCov(nObs*nCovs);
    
    IntegerVector newEvid(nDoses);
    NumericVector newAmt(nDoses);
    NumericVector newTimeA(nDoses);
    lastId = id[0]-1;
  
    IntegerVector newId(nSub);
    IntegerVector posDose(nSub);
    IntegerVector posObs(nSub);
    IntegerVector posCov(nSub);
    IntegerVector nCov(nSub);
    IntegerVector nDose(nSub);
    IntegerVector nObsN(nSub);
    IntegerVector posEt(nSub);
    IntegerVector nEtN(nSub);
    NumericVector Hmax(nSub); // For LSODA default
    IntegerVector rc(nSub);
    
    double minTime = NA_REAL;
    double maxTime = -1e10;
    double minIdTime = NA_REAL;
    double lastTime = 0;
    //hmax <- max(abs(diff(event.table$time)))
    double mdiff = 0;
    double tmp;
    int m = 0, nEt=0;
    for (i = 0; i < ids; i++){
      if (lastId != id[i]){
        lastId     = id[i];
        newId[m]   = id[i];
        posDose[m] = j;
        posObs[m]  = k;
        posEt[m] = i;
        if (m != 0){
          nDose[m-1] = nDoses;
          nObsN[m-1]  = nObs;
          nEtN[m-1] = nEt;
	  Hmax[m-1] = mdiff;
	  rc[m-1] = 0;
        }
        nDoses = 0;
        nObs = 0;
        nEt  = 0;
        m++;
	minIdTime = time0[i];
	lastTime = time0[i];
	mdiff = 0;
      }
      if (minIdTime > time0[i]){
	stop("Data need to be ordered by ID and TIME.");
      } else {
	minIdTime = time0[i];
      }
      tmp = time0[i]-lastTime;
      if (tmp > mdiff){
	mdiff = tmp;
      }
      lastTime = time0[i];
      if (evid[i]){
        // Dose
        newEvid[j]  = evid[i];
        newTimeA[j] = time0[i];
        newAmt[j]   = amt[i];
        nDoses++;
        j++;
        nEt++;
      } else {
        // Observation
        newDv[k]    = dv[i];
        newTimeO[k] = time0[i];
        if (nObs == 0){
          minTime = time0[i];
        } else if (time0[i] > maxTime) {
          maxTime = time0[i];
        }
        nObs++;
        k++;
        nEt++;
      }
    }
    nDose[m-1]=nDoses;
    nObsN[m-1]=nObs;
    nEtN[m-1] = nEt;
    Hmax[m-1] = mdiff;
    rc[m-1] = 0;
    if (dataCov && covDf.nrow() != nObs){
      stop("Covariate data needs to match the number of observations in the overall dataset.");
    }
    // Covariates are stacked by id that is
    // id=cov1,cov1,cov1,cov2,cov2,cov2,...
    lastId = id[0]-1;
    int n0 = 0, n = 0, nc = 0;
    m = 0;
    for (i = 0; i < ids; i++){
      if (lastId != id[i]){
        lastId     = id[i];
        if (m != 0){
          n0 += nObsN[m-1]*nCovs;
        }
        posCov[m] = n0;
        nCov[m]   = nObsN[m]*nCovs;
        nc = 0;
        m++;
      }
      if (!evid[i]){
        // Observation
        for (n = 0; n < nCovs; n++){
	  k = n0 + nc + n*nObsN[m-1];
	  if (n < nCovObs){
	    if (dataCov){
              newCov[k] = (as<NumericVector>(covDf[as<std::string>(covN[n])]))[nc];
            } else {
              newCov[k] = (as<NumericVector>(dataf[as<std::string>(covN[n])]))[i];
            }
          } else {
	    newCov[k] = simMat(nc, n-nCovObs);
          }
        }
        nc++;
      }
    }
    // nCov[m-1] = nObs*nCovs;
    List ret = List::create(_["dose"] = DataFrame::create(_["evid"]         = newEvid,
                                                          _["time"]         = newTimeA,
                                                          _["amt"]          = newAmt),
                            _["obs"]  = DataFrame::create(_["dv"]           = newDv,
                                                          _["time"]         = newTimeO),
                            _["ids"]  = DataFrame::create(_["id"]           = newId,
                                                          _["posDose"]      = posDose,
                                                          _["posObs"]       = posObs,
                                                          _["posCov"]       = posCov,
                                                          _["posEvent"]     = posEt,
                                                          _["nDose"]        = nDose,
                                                          _["nObs"]         = nObsN,
                                                          _["nCov"]         = nCov,
                                                          _["nEvent"]       = nEtN,
							  _["HmaxDefault"]  = Hmax,
							  _["rc"]           = rc),
                            _["et"] = DataFrame::create(_["evid"]=evid,
                                                        _["time"]=time0),
                            _["cov"]=newCov,
                            _["nSub"]=nSub,
                            _["nDoses"]=newEvid.size(),
                            _["nObs"]=newDv.size(),
                            _["min.time"] = minTime,
                            _["max.time"] = maxTime,
                            _["cov.names"]=(covN.size() == 0 ? R_NilValue : wrap(covN)),
			    _["n.observed.covariates"] = nCovObs,
			    _["simulated.vars"] = (simNames.size()== 0 ? R_NilValue : wrap(simNames)),
			    _["sigma"] = wrap(sigma),
                            _["amount.units"]=(as<std::string>(amountUnits) == "NA") ? StringVector::create(NA_STRING) : amountUnits,
                            _["time.units"]=(as<std::string>(timeUnits)  == "NA") ? StringVector::create(NA_STRING) : timeUnits,
			    _["missing.id"]=missingId,
			    _["missing.dv"]=missingDv,
			    _["ncoresRV"] = wrap(ncoresRV),
			    _["isChol"] = wrap(isChol)
                            );
    // Not sure why, but putting this in above gives errors...
    ret["df"]= df;
    ret.attr("class") = "RxODE.multi.data";
    return ret;
  } else {
    stop("Data is not setup appropriately.");
  }
}

//' Update RxODE multi-subject data with new residuals (in-place).
//'
//' @param multiData The RxODE multi-data object setup from \code{\link{rxDataSetup}}
//'
//' @return An integer indicating if this is object has residuals that are updating (0 for no-residuals; 1 for residuals).
//'        
//' @author Matthew L. Fidler
//' @keywords internal
//' @export
//[[Rcpp::export]]
bool rxUpdateResiduals(List &multiData){
  if (rxIs(multiData, "RxODE.multi.data")){
    RObject sigma = multiData["sigma"];
    if (!sigma.isNULL()){
      int totNObs = as<int>(multiData["nObs"]);
      RObject df = multiData["df"];
      int ncores = as<int>(multiData["ncoresRV"]);
      bool isChol = as<bool>(multiData["isChol"]);
      RObject tmp_ro = rxSimSigma(sigma, df, ncores, isChol, totNObs);
      if (!tmp_ro.isNULL()){
	// Resimulated; now fill in again...
        NumericMatrix simMat = as<NumericMatrix>(tmp_ro);
	SEXP cov_ = multiData["cov"];
        NumericVector cov = NumericVector(cov_);
	DataFrame ids = as<DataFrame>(multiData["ids"]);
	IntegerVector posCov = ids["posCov"];
	IntegerVector nCov   = ids["nCov"];
	IntegerVector nObs   = ids["nObs"];
	int nObsCov = as<int>(multiData["n.observed.covariates"]);
	int nSimCov = simMat.ncol();
	int nc = 0;
	for (int id = 0; id < posCov.size(); id++){
	  for (int no = 0; no < nObs[id]; no++){
	    for (int ns = 0; ns < nSimCov; ns++){
              cov[posCov[id]+no+(nObsCov+ns)*nObs[id]] = simMat(nc, ns);
	    }
	    nc++;
	  }
	}
	return true;
      }
      return false;
    }
    return false;
  }
  return false;
}


extern "C" int rxUpdateResiduals_(SEXP md);
int rxUpdateResiduals_(SEXP md){
  List mdl = List(md);
  bool ret = rxUpdateResiduals(mdl);
  if (ret)
    return 1;
  else
    return 0;
}

extern "C" rx_solve *getRxSolve(SEXP ptr);
rx_solve *getRxSolve(SEXP ptr){
  if (rxIs(ptr,"RxODE.pointer.multi")){
    List lst = List(ptr);
    SEXP ptr = as<SEXP>(lst["pointer"]);
    rx_solve *ret = getRxSolve_(ptr);
    // Also assign it.
    SEXP setS = as<SEXP>(lst["set_solve"]);
    t_set_solve set_solve = (t_set_solve)R_ExternalPtrAddr(setS);
    set_solve(ret);
    return ret;
  } else {
    stop("Cannot get the solving data (getRxSolve).");
  }
  rx_solve *o;
  return o;
}

extern "C" void freeRxSolve(SEXP ptr);
void freeRxSolve(SEXP ptr){
  List lst = List(ptr);
  SEXP pt = as<SEXP>(lst["pointer"]);
  rxSolveDataFree(pt);
}

//' All model variables for a RxODE object
//'
//' Return all the known model variables for a specified RxODE object
//'
//' These items are only calculated after compilation; they are
//' built-into the RxODE compiled DLL.
//'
//' @param obj RxODE family of objects
//'
//' @return A list of RxODE model properties including:
//'
//' \item{params}{ a character vector of names of the model parameters}
//' \item{lhs}{ a character vector of the names of the model calculated parameters}
//' \item{state}{ a character vector of the compartments in RxODE object}
//' \item{trans}{ a named vector of translated model properties
//'       including what type of jacobian is specified, the \code{C} function prefixes,
//'       as well as the \code{C} functions names to be called through the compiled model.}
//' \item{md5}{a named vector that gives the digest of the model (\code{file_md5}) and the parsed model
//'      (\code{parsed_md5})}
//' \item{model}{ a named vector giving the input model (\code{model}),
//'    normalized model (no comments and standard syntax for parsing, \code{normModel}),
//'    and interim code that is used to generate the final C file \code{parseModel}}
//'
//' @keywords internal
//' @author Matthew L.Fidler
//' @export
// [[Rcpp::export]]
List rxModelVars(const RObject &obj){
  if (rxIs(obj, "rxModelVars")){
    List ret(obj);
    return ret;
  } else if (rxIs(obj,"RxODE")) {
    Function f = as<Function>((as<List>((as<List>(obj))["cmpMgr"]))["rxDll"]);
    List lst = f();
    return lst["modVars"];
  } else if (rxIs(obj,"solveRxODE7")){
    CharacterVector cls = obj.attr("class");
    Environment e = as<Environment>(cls.attr(".RxODE.env"));
    return  rxModelVars(as<RObject>(e["args.object"]));
  } else if (rxIs(obj,"rxDll")){
    List lobj = (as<List>(obj))["modVars"];
    return lobj;
  } else if (rxIs(obj, "RxCompilationManager")){
    Function f =  as<Function>((as<List>(obj))["rxDll"]);
    List lst = f();
    return (lst["modVars"]);
  } else if (rxIs(obj, "character")){
    Environment RxODE("package:RxODE");
    Function f = as<Function>(RxODE["rxModelVars.character"]);
    return f(obj);
  } else if (rxIs(obj,"list")){
    bool params=false, lhs=false, state=false, trans=false, ini=false, model=false, md5=false, podo=false, dfdy=false;
    List lobj  = as<List>(obj);
    CharacterVector nobj = lobj.names();
    for (int i = 0; i < nobj.size(); i++){
      if (nobj[i] == "modVars"){
	return(rxModelVars(lobj["modVars"]));
      } else if (!params && nobj[i]== "params"){
	params=true;
      } else if (!lhs && nobj[i] == "lhs"){
	lhs=true;
      } else if (!state && nobj[i] == "state"){
	state=true;
      } else if (!trans && nobj[i] == "trans"){
	trans=true;
      } else if (!ini && nobj[i] == "ini"){
	ini = true;
      } else if (!model && nobj[i] == "model"){
	model = true;
      } else if (!md5 && nobj[i] == "md5"){
	md5 = true;
      } else if (!podo && nobj[i] == "podo"){
	podo=true;
      } else if (!dfdy && nobj[i] == "dfdy"){
	dfdy = true;
      } else {
	return lobj;
      }
    }
    stop("Cannot figure out the model variables.");
  } else {
    stop("Need an RxODE-type object to extract model variables from.");
  }
}
//' State variables
//'
//' This returns the model's compartments or states.
//'
//' @inheritParams rxModelVars
//'
//' @param state is a string indicating the state or compartment that
//'     you would like to lookup.
//'
//' @return If state is missing, return a character vector of all the states.
//'
//' If state is a string, return the compartment number of the named state.
//'
//' @seealso \code{\link{RxODE}}
//'
//' @author Matthew L.Fidler
//' @export
// [[Rcpp::export]]
RObject rxState(const RObject &obj = R_NilValue, RObject state = R_NilValue){
  List modVar = rxModelVars(obj);
  CharacterVector states = modVar["state"];
  if (state.isNULL()){
    return states;
  }
  else if (rxIs(state,"character")){
    CharacterVector lookup = as<CharacterVector>(state);
    if (lookup.size() > 1){
      // Fixme?
      stop("Can only lookup one state at a time.");
    }
    if (states.size() == 1){
      warning("Only one state variable should be input.");
    }
    IntegerVector ret(1);
    for (int i = 0; i < states.size(); i++){
      if (states[i] == lookup[0]){
	ret[0] = i+1;
	return ret;
      }
    }
    stop("Cannot locate compartment \"%s\".",as<std::string>(lookup[0]).c_str());
  }
  return R_NilValue;
}

//' Parameters specified by the model
//'
//' This return the model's parameters that are required to solve the
//' ODE system.
//'
//' @inheritParams rxModelVars
//'
//' @return a character vector listing the parameters in the model.
//'
//' @author Matthew L.Fidler
//' @export
//[[Rcpp::export]]
CharacterVector rxParams(const RObject &obj){
  List modVar = rxModelVars(obj);
  CharacterVector ret = modVar["params"];
  return ret;
}


//' Jacobian and parameter derivatives
//'
//' Return Jacobain and parameter derivatives
//'
//' @inheritParams rxModelVars
//'
//' @return A list of the jacobian parameters defined in this RxODE
//'     object.
//' @author Matthew L. Fidler
//' @export
//[[Rcpp::export]]
CharacterVector rxDfdy(const RObject &obj){
  List modVar = rxModelVars(obj);
  CharacterVector ret = modVar["dfdy"];
  return ret;
}

//' Left handed Variables
//'
//' This returns the model calculated variables
//'
//' @inheritParams rxModelVars
//'
//' @return a character vector listing the calculated parameters
//' @seealso \code{\link{RxODE}}
//'
//' @author Matthew L.Fidler
//' @export
//[[Rcpp::export]]
CharacterVector rxLhs(const RObject &obj){
  List modVar = rxModelVars(obj);
  CharacterVector ret = modVar["lhs"];
  return ret;
}

//' Initial Values and State values for a RxODE object
//'
//' Returns the initial values of the rxDll object
//'
//' @param obj rxDll, RxODE, or named vector representing default
//'     initial arguments
//'
//' @param vec If supplied, named vector for the model.
//'
//' @param req Required names, and the required order for the ODE solver
//'
//' @param defaultValue a number or NA representing the default value for
//'     parameters missing in \code{vec}, but required in \code{req}.
//'
//' @param noerror is a boolean specifying if an error should be thrown
//'     for missing parameter values when \code{default} = \code{NA}
//'
//' @keywords internal
//' @author Matthew L.Fidler
//' @export
//[[Rcpp::export]]
NumericVector rxInits(const RObject &obj,
		      Nullable<NumericVector> vec = R_NilValue,
		      Nullable<CharacterVector> req = R_NilValue,
		      double defaultValue = 0,
		      bool noerror = false,
		      bool noini=false){
  NumericVector oini;
  CharacterVector cini;
  List modVar = rxModelVars(obj);
  if (!noini){
    oini = (modVar["ini"]);
    cini = oini.names();
  }
  int i, j, k;
  CharacterVector nreq;
  NumericVector miss;
  if (!req.isNull()){
    nreq = CharacterVector(req);
    if ((ISNA(defaultValue) && noerror) || !ISNA(defaultValue)){
      miss = NumericVector(nreq.size());
      for (i = 0; i < nreq.size(); i++) {
	miss[i] = defaultValue;
      }
      miss.attr("names") = CharacterVector(nreq);
    }
  }
  NumericVector nvec;
  CharacterVector nvecNames;
  if (!vec.isNull()){
    nvec = NumericVector(vec);
    if (nvec.size() > 0){
      if (!nvec.hasAttribute("names")){
	if (!req.isNull() && nreq.size() == nvec.size()){
	  nvec.attr("names") = req;
	  nvecNames = req;
	  std::string wstr = "Assumed order of inputs: ";
	  for (i = 0; i < nreq.size(); i++){
	    wstr += (i == 0 ? "" : ", ") + nreq[i];
	  }
	  warning(wstr);
	} else {
	  std::string sstr = "Length mismatch\nreq: c(";
	  for (i = 0; i < nreq.size(); i++){
	    sstr += (i == 0 ? "" : ", ") + nreq[i];
	  }
	  sstr += ")\nvec: c(";
	  for (i = 0; i < nvec.size(); i++){
            sstr += (i == 0 ? "" : ", ") + std::to_string((double)(nvec[i]));
          }
	  sstr += ")";
	  stop(sstr);
	}
      } else {
	nvecNames = nvec.names();
      }
    }
  }
  // Prefer c(vec, ini, miss)
  NumericVector ret;
  CharacterVector nret;
  if (!req.isNull()){
    ret =  NumericVector(nreq.size());
    bool found = false;
    for (i = 0; i < nreq.size(); i++){
      found = false;
      for (j = 0; !found && j < nvec.size(); j++){
	if (nreq[i] == nvecNames[j]){
	  found =  true;
	  ret[i] = nvec[j];
	  break;
	}
      }
      for (j = 0; !found && j < cini.size(); j++){
	if (nreq[i] == cini[j]){
          found =  true;
          ret[i] = oini[j];
          break;
        }
      }
      if (!found)
	ret[i] = miss[i];
    }
    ret.attr("names")= nreq;
  } else {
    // In this case
    // vec <- c(vec, ini);
    // vec <- vec[!duplicated(names(vec))]
    CharacterVector dupnames(nvec.size()+oini.size()+miss.size());
    j = 0;
    for (i = 0; i < nvec.size(); i++){
      dupnames[j] = nvecNames[i];
      j++;
    }
    for (i = 0; i < oini.size(); i++){
      dupnames[j] = cini[i];
      j++;
    }
    LogicalVector dups = duplicated(dupnames);
    j = 0;
    for (i = 0; i < dups.size(); i++){
      if (!dups[i]) j++;
    }
    ret = NumericVector(j);
    CharacterVector retn(j);
    k = 0, j = 0;
    for (i = 0; i < nvec.size(); i++){
      if (!dups[j]){
	ret[k] = nvec[i];
	retn[k] = nvecNames[i];
	k++;
      }
      j++;
    }
    for (i = 0; i < oini.size(); i++){
      if (!dups[j]){
	ret[k] = oini[i];
        retn[k] = cini[i];
        k++;
      }
      j++;
    }
    ret.attr("names") = retn;
  }
  return ret;
}
//' Setup the initial conditions.
//'
//' @param obj RxODE object
//' @param inits A numeric vector of initial conditions.
//' @author Matthew L. Fidler
//' @keywords internal
//' @export
//[[Rcpp::export]]
NumericVector rxSetupIni(const RObject &obj,
			   Nullable<NumericVector> inits = R_NilValue){
  List modVars = rxModelVars(obj);
  CharacterVector state = modVars["state"];
  return rxInits(obj, inits, state, 0.0);
}

//' Setup Data and Parameters
//'
//' @inheritParams rxSolve
//' @param sigma Named sigma matrix.
//' @param sigmaDf The degrees of freedom of a t-distribution for
//'     simulation.  By default this is \code{NULL} which is
//'     equivalent to \code{Inf} degrees, or to simulate from a normal
//'     distribution instead of a t-distribution.
//' @param sigmaNcores Number of cores for residual simulation.  This,
//'     along with the seed, affects both the outcome and speed of
//'     simulation. By default it is one.
//' @param sigmaIsChol Indicates if the \code{sigma} supplied is a
//'     Cholesky decomposed matrix instead of the traditional
//'     symmetric matrix.
//' @return Data setup for running C-based RxODE runs.
//' @author Matthew L. Fidler
//' @keywords internal
//' @export
//[[Rcpp::export]]
List rxDataParSetup(const RObject &object,
		    const RObject &params = R_NilValue,
		    const RObject &events = R_NilValue,
		    const Nullable<NumericVector> &inits = R_NilValue,
		    const RObject &covs  = R_NilValue,
		    const RObject &sigma= R_NilValue,
		    const RObject &sigmaDf= R_NilValue,
		    const int &sigmaNcores= 1,
		    const bool &sigmaIsChol= false,
		    const StringVector &amountUnits = NA_STRING,
		    const StringVector &timeUnits = "hours"){
  List modVars = rxModelVars(object);
  CharacterVector state = modVars["state"];
  // The initial conditions cannot be changed for each individual; If
  // they do they need to be a parameter.
  NumericVector initsC = rxInits(object, inits, state, 0.0);
  // The parameter vector/matrix/data frame contains the parameters
  // that will be used.
  RObject par0 = params;
  RObject ev0  = events;
  RObject ev1;
  RObject par1;
  if (rxIs(par0, "rx.event")){
    // Swapped events and parameters
    ev1 = par0;
    par1 = ev0;
  } else if (rxIs(ev0, "rx.event")) {
    ev1 = ev0;
    par1 = par0;
  } else {
    stop("Need some event information (observation/dosing times) to solve.\nYou can use either 'eventTable' or an RxODE compatible data frame/matrix.");
  }
  // Now get the parameters (and covariates)
  //
  // Unspecified parameters can be found in the modVars["ini"]
  NumericVector modVarsIni = modVars["ini"];
  // The event table can contain covariate information, if it is acutally a data frame or matrix.
  Nullable<CharacterVector> covnames0, simnames0;
  CharacterVector covnames, simnames;
  CharacterVector pars = modVars["params"];
  int i, j, k = 0;
  CharacterVector tmpCv;
  List ret;
  if (!rxIs(ev1,"EventTable") &&  covs.isNULL()){
    // Now covnames is setup correctly, import into a setup data table.
    // In this case the events are a data frame or matrix
    CharacterVector tmpCv =as<CharacterVector>((as<DataFrame>(ev1)).names());
    for (i = 0; i < pars.size(); i++){
      for (j = 0; j < tmpCv.size(); j++){
	if (pars[i] == tmpCv[j]){
	  k++;
	  break;
	}
      }
    }
    covnames = CharacterVector(k);
    k = 0;
    for (i = 0; i < pars.size(); i++){
      for (j = 0; j < tmpCv.size(); j++){
	if (pars[i] == tmpCv[j]){
	  covnames[k] = pars[i];
	  k++;
	  break;
	}
      }
    }
    ret = rxDataSetup(ev1, (covnames.size() == 0 ? R_NilValue : wrap(covnames)),
		      sigma, sigmaDf, sigmaNcores, sigmaIsChol, amountUnits, timeUnits);
  } else {
    ret = rxDataSetup(ev1, covs, sigma, sigmaDf,
		      sigmaNcores, sigmaIsChol, amountUnits, timeUnits);
    covnames0 = as<Nullable<CharacterVector>>(ret["cov.names"]);
    if (!covnames0.isNull()){
      covnames = CharacterVector(covnames0);
    }
  }
  simnames0 = as<Nullable<CharacterVector>>(ret["simulated.vars"]);
  if (!simnames0.isNull()){
    simnames = CharacterVector(simnames);
  }
  // Now get the parameters as a data.frame
  NumericMatrix parMat;
  if (par1.isNULL()){
  } else if (rxIs(par1, "data.frame") || rxIs(par1, "matrix")){
    if (rxIs(par1,"data.frame")){
      DataFrame tmp = as<DataFrame>(par1);
      int nr = tmp.nrows();
      NumericMatrix tmpM(nr,tmp.size());
      for (i = 0; i < tmp.size(); i++){
	tmpM(_,i) = NumericVector(tmp[i]);
      }
      tmpM.attr("dimnames") = List::create(R_NilValue,tmp.names());
      parMat=tmpM;
    } else {
      parMat = as<NumericMatrix>(par1);
    }
  } else if (rxIs(par1, "numeric") || rxIs(par1, "integer")){
    // Create the matrix
    NumericVector tmp0 = as<NumericVector>(par1);
    NumericMatrix tmp1(1, tmp0.size());
    for (i = 0; i < tmp0.size(); i++){
      tmp1(0,i) = tmp0[i];
    }
    if (tmp0.hasAttribute("names")){
      tmp1.attr("dimnames") = List::create(R_NilValue, tmp0.names());
    } else if (tmp0.size() == pars.size()){
      tmp1.attr("dimnames") = List::create(R_NilValue, pars);
    } else {
      // In this case there are no names
      stop("The parameter names must be specified.");
    }
    parMat = tmp1;
  }
  int nSub = as<int>(ret["nSub"]);
  if (parMat.nrow() % nSub != 0){
    stop("The Number of parameters must be a multiple of the number of subjects.");
  }
  k = 0;
  IntegerVector pcov(covnames.size()+simnames.size());
  for (i = 0; i < covnames.size(); i++){
    for (j = 0; j < pars.size(); j++){
      if (covnames[i] == pars[j]){
	pcov[i] = j + 1;
	break;
      }
    }
  }
  for (i = 0; i < simnames.size(); i++){
    for (j = 0; j < pars.size(); j++){
      if (simnames[i] == pars[j]){
        pcov[i] = j + 1;
        break;
      }
    }
  }
  // Now pcov gives the which for the covariate parameters.
  // Now check if we have all the parameters that are needed.
  std::string errStr = "";
  bool allPars = true;
  bool curPar = false;
  IntegerVector posPar(pars.size());
  CharacterVector nms = modVarsIni.names();
  Nullable<CharacterVector> nmP2 = (as<List>(parMat.attr("dimnames")))[1];
  CharacterVector nmP;
  if (!nmP2.isNull()){
    nmP = CharacterVector(nmP2);
  }
  for (i = 0; i < pars.size(); i++){
    curPar = false;
    // integers are faster to compare than strings.
    for (j = 0; j < pcov.size(); j++){
      if (pcov[j] == i + 1){
	posPar[i] = 0; // Covariates are zeroed out.
	curPar = true;
	break;
      }
    }
    // First Check to see if the user specified the parameter.
    if (!curPar){
      for (j = 0; j < nmP.size(); j++){
        if (nmP[j] == pars[i]){
          curPar = true;
          posPar[i] = j + 1;
          break;
        }
      }
    }
    // Now check $ini
    if (!curPar){
      for(j = 0; j < modVarsIni.size(); j++){
        if (nms[j] == pars[i]){
          curPar = true;
          posPar[i] = -j - 1;
          break;
        }
      }
    }
    if (!curPar){
      if (allPars){
	errStr = "The following parameter(s) are required for solving: " + pars[i];
      } else {
	errStr = ", " + pars[i];
      }
      allPars = false;
    }
  }
  if (!allPars){
    stop(errStr);
  }
  // Now  the parameter names are setup.
  // The parameters are setup in a numeric vector in order of pars
  int nr = parMat.nrow();
  if (nr == 0) nr = 1;
  NumericVector parsVec(pars.size()*nr);
  j = 0;
  for (i = 0; i < parsVec.size(); i++){
    j = floor(i / pars.size());
    k = i % pars.size();
    if (posPar[k] == 0){
      parsVec[i] = 0;
    } else if (posPar[k] > 0){
      // posPar[i] = j + 1;
      parsVec[i] = parMat(j, posPar[k]-1);
    } else {
      // posPar[i] = -j - 1;
      parsVec[i] = modVarsIni[-(posPar[k]+1)];
    }
  }
  ret["pars"] = parsVec;
  nr = parMat.nrow() / nSub;
  if (nr == 0) nr = 1;
  ret["nsim"] = nr;
  // NumericVector initsS = NumericVector(initsC.size()*nSub*nr);
  // for (i = 0; i < initsS.size(); i++){
  //   j = i % initsS.size();
  //   initsS[i] =  initsC[j];
  // }
  ret["inits"] = initsC;
  // ret["inits.full"] = initsS;
  ret["n.pars"] = (int)(pars.size());
  ret["pcov"] = pcov;
  ret["neq"] = state.size();
  DataFrame et      = as<DataFrame>(ret["et"]);
  NumericVector solve(state.size()*et.nrow()*nr);
  ret["solve"] = solve;
  IntegerVector idose(et.nrow()*nr);
  ret["idose"] = idose;
  CharacterVector lhs = as<CharacterVector>(modVars["lhs"]);
  ret["lhs"] = NumericVector(lhs.size()*nSub*nr);
  ret["lhsSize"] = lhs.size();
  ret["InfusionRate"] =NumericVector(state.size()*nSub*nr);
  ret["BadDose"] =IntegerVector(state.size()*nSub*nr);
  ret["state.ignore"] = modVars["state.ignore"];
  CharacterVector cls(2);
  cls(1) = "RxODE.par.data";
  cls(0) = "RxODE.multi.data";
  ret.attr("class") = cls;
  return ret;
}

extern "C" {
  SEXP getSolvingOptionsPtr(double ATOL,          //absolute error
			    double RTOL,          //relative error
			    double H0,
			    double HMIN,
			    int global_jt,
			    int global_mf,
			    int global_debug,
			    int mxstep,
			    int MXORDN,
			    int MXORDS,
			    // Approx options
			    int do_transit_abs,
			    int nlhs,
			    int neq,
			    int stiff,
			    double f1,
                            double f2,
                            int kind,
                            int is_locf,
			    int cores,
			    int ncov,
			    int *par_cov,
                            int do_par_cov,
			    double *inits,
                            SEXP stateNames,
			    SEXP lhsNames,
			    SEXP paramNames,
                            SEXP dydt,
                            SEXP calc_jac,
                            SEXP calc_lhs,
			    SEXP update_inis,
			    SEXP dydt_lsoda_dum,
			    SEXP jdum_lsoda);

  void getSolvingOptionsIndPtr(double *InfusionRate,
			       int *BadDose,
			       double HMAX, // Determined by diff
			       double *par_ptr,
			       double *dose,
			       double *solve,
			       double *lhs,
			       int  *evid,
			       int *rc,
			       double *cov_ptr,
			       int n_all_times,
			       double *all_times,
			       int id,
			       int sim,
			       rx_solving_options_ind *o);

  SEXP rxSolveData(rx_solving_options_ind *subjects,
		   int nsub,
		   int nsim,
		   int *stateIgnore,
		   int nobs,
		   int add_cov,
		   int matrix,
		   SEXP op);
}

SEXP rxSolvingOptions(const RObject &object,
                      const std::string &method = "lsoda",
                      const Nullable<LogicalVector> &transit_abs = R_NilValue,
                      const double atol = 1.0e-8,
                      const double rtol = 1.0e-6,
                      const int maxsteps = 5000,
                      const int hmin = 0,
		      const int hini = 0,
		      const int maxordn = 12,
                      const int maxords = 5,
		      const int cores = 1,
		      const int ncov = 0,
		      int *par_cov = NULL,
		      int do_par_cov = 0,
		      double *inits = NULL,
		      std::string covs_interpolation = "linear"){
  if (maxordn < 1 || maxordn > 12){
    stop("'maxordn' must be >1 and <= 12.");
  }
  if (maxords < 1 || maxords > 5){
    stop("'maxords' must be >1 and <= 5.");
  }
  if (hmin < 0){
    stop("'hmin' must be a non-negative value.");
  }
  // HMAX is determined by the problem since it can be thought of as the maximum difference of the event table's time
  if (hini < 0){
    stop("'hini' must be a non-negative value.");
  }
  List modVars = rxModelVars(object);
  List ptr = modVars["ptr"];
  int transit = 0;
  if (transit_abs.isNull()){
    transit = modVars["podo"];
    if (transit){
      warning("Assumed transit compartment model since 'podo' is in the model.");
    }
  } else {
    LogicalVector tr = LogicalVector(transit_abs);
    if (tr[0]){
      transit=  1;
    }
  }
  int is_locf = 0;
  double f1 = 1.0, f2 = 0.0;
  int kind=1;
  if (covs_interpolation == "linear"){
  } else if (covs_interpolation == "constant" || covs_interpolation == "locf" || covs_interpolation == "LOCF"){
    f2 = 0.0;
    f1 = 1.0;
    kind = 0;
    is_locf=1;
  } else if (covs_interpolation == "nocb" || covs_interpolation == "NOCB"){
    f2 = 1.0;
    f1 = 0.0;
    kind = 0;
    is_locf=2;
  }  else if (covs_interpolation == "midpoint"){
    f1 = f2 = 0.5;
    kind = 0;
    is_locf=3;
  } else {
    stop("Unknown covariate interpolation specified.");
  }
  int st=0;
  if (method == "lsoda"){
    st = 1;
  } else if (method == "dop"){
    st = 0;
  } else {
    stop("Unknown ODE solving method specified.");
  }
  CharacterVector lhs = as<CharacterVector>(modVars["lhs"]);
  CharacterVector state = as<CharacterVector>(modVars["state"]);
  CharacterVector params = as<CharacterVector>(modVars["params"]);
  return getSolvingOptionsPtr(atol,rtol,hini, hmin,
			      as<int>(ptr["jt"]),
			      as<int>(ptr["mf"]),
			      as<int>(ptr["debug"]),
			      maxsteps, maxordn, maxords, transit,
			      lhs.size(), state.size(),
			      st, f1, f2, kind, is_locf, cores,
			      ncov,par_cov, do_par_cov, &inits[0],
			      as<SEXP>(state), as<SEXP>(lhs),
			      as<SEXP>(params),
                              as<SEXP>(ptr["dydt"]),
                              as<SEXP>(ptr["jac"]),
			      as<SEXP>(ptr["lhs"]),
			      as<SEXP>(ptr["inis"]),
			      as<SEXP>(ptr["dydt_lsoda"]),
			      as<SEXP>(ptr["jdum"]));
}

//[[Rcpp::export]]
SEXP rxSolvingData(const RObject &model,
		   const RObject &parData,
		   const std::string &method = "lsoda",
		   const Nullable<LogicalVector> &transit_abs = R_NilValue,
		   const double atol = 1.0e-8,
		   const double rtol = 1.0e-6,
		   const int maxsteps = 5000,
		   const int hmin = 0,
		   const Nullable<NumericVector> &hmax = R_NilValue,
		   const int hini = 0,
		   const int maxordn = 12,
		   const int maxords = 5,
		   const int cores = 1,
		   std::string covs_interpolation = "linear",
		   bool addCov = false,
		   bool matrix = false) {
  if (rxIs(parData, "RxODE.par.data")){
    List opt = List(parData);
    DataFrame ids = as<DataFrame>(opt["ids"]);
    IntegerVector BadDose = as<IntegerVector>(opt["BadDose"]);
    NumericVector InfusionRate = as<NumericVector>(opt["InfusionRate"]);
    NumericVector par = as<NumericVector>(opt["pars"]);
    double hm;
    int nPar = as<int>(opt["n.pars"]);
    int nSub = as<int>(opt["nSub"]);
    NumericVector inits = as<NumericVector>(opt["inits"]);
    DataFrame doseDf = as<DataFrame>(opt["dose"]);
    NumericVector amt = as<NumericVector>(doseDf["amt"]);
    IntegerVector posDose = as<IntegerVector>(ids["posDose"]);
    IntegerVector posEvent = as<IntegerVector>(ids["posEvent"]);
    IntegerVector posCov = as<IntegerVector>(ids["posCov"]);
    IntegerVector nEvent = as<IntegerVector>(ids["nEvent"]);
    NumericVector solve = as<NumericVector>(opt["solve"]);
    DataFrame et      = as<DataFrame>(opt["et"]);
    IntegerVector evid  = as<IntegerVector>(et["evid"]);
    NumericVector all_times = as<NumericVector>(et["time"]);
    int totSize = et.nrow();
    NumericVector lhs = as<NumericVector>(opt["lhs"]);
    int lhsSize = as<int>(opt["lhsSize"]);
    IntegerVector par_cov = as<IntegerVector>(opt["pcov"]);
    NumericVector cov = as<NumericVector>(opt["cov"]);
    IntegerVector rc=as<IntegerVector>(ids["rc"]);
    IntegerVector siV = as<IntegerVector>(opt["state.ignore"]);
    int do_par_cov = 0;
    if (par_cov.size() > 0){
      do_par_cov = 1;
    }
    rx_solving_options_ind *inds;
    int nsim = as<int>(opt["nsim"]);
    inds = Calloc(nSub*nsim,rx_solving_options_ind);
    int neq = as<int>(opt["neq"]);
    int ncov =-1;
    int cid;
    for (int simNum = 0; simNum < nsim; simNum++){
      for (int id = 0; id < nSub; id++){
	cid = id+simNum*nSub;
	if (hmax.isNull()){
          // Get from data.
          NumericVector hmn = as<NumericVector>(ids["HmaxDefault"]);
          hm = hmn[id];
        } else {
          NumericVector hmn = NumericVector(hmax);
          if (R_FINITE(hmn[0])){
            if (hmn[0] < 0)
	      Free(inds);
              stop("'hmax' must be a non-negative value.");
            hm = hmn[0];
          } else {
            hm = 0.0;
          }
        }
	ncov = par_cov.size();
        getSolvingOptionsIndPtr(&InfusionRate[cid*neq],&BadDose[cid*neq], hm,&par[cid*nPar],
                                &amt[posDose[id]],
                                // Solve and lhs are written to in the solve...
                                &solve[cid*totSize*neq],
                                &lhs[cid*lhsSize],
                                // Doesn't change with the solve.
				&evid[posEvent[id]], &rc[id], &cov[posCov[id]],
                                nEvent[id], &all_times[posEvent[id]], id, simNum,
                                &inds[cid]);
      }
    }
    SEXP op = rxSolvingOptions(model,method, transit_abs, atol, rtol, maxsteps, hmin, hini, maxordn,
			       maxords, cores, ncov, &par_cov[0], do_par_cov, &inits[0], covs_interpolation);
    int add_cov = 0;
    if (addCov) add_cov = 1;
    int nobs = as<int>(opt["nObs"]);
    int mat = 0;
    if (matrix) mat = 1;
    return rxSolveData(inds, nSub, nsim, &siV[0], nobs, add_cov, mat, op);
  } else {
    stop("This requires something setup by 'rxDataParSetup'.");
  }
  return R_NilValue;
}

//' Setup a data frame for solving multiple subjects at once in RxODE.
//'
//' @param object R object to setup; Must be in RxODE compatible format.
//' @inheritParams rxSolve
//' @param covNames Covariate names in dataset.
//' @param sigma Matrix for simulating residual variability for the number of observations.
//'      This uses the \code{\link[mvnfast]{rmvn}} or \code{\link[mvnfast]{rmvt}} assuming mean 0 and covariance given by this named
//'      matrix.  The residual-type variability is created for each of the named \"err\" components specified by the sigma matrix's
//'      column names. This then creates a random deviate that is used in the place of each named variable.  This should
//'      not really be used in the ODE specification.  If it is, though, it is treated as a time-varying covariate.
//'
//'      Also note this does not use R's random numbers, rather it uses a cryptographic parallel random number generator;
//'
//'      To allow reproducible research you must both set a random seed with R's \code{\link[base]{set.seed}} function, and keep the
//'      number of cores constant.  By changing either one of these variables, you will arrive at different random numbers. 
//' @param df Degrees of freedom for \code{\link[mvnfast]{rmvt}}.  If \code{NULL}, or \code{Inf}, then use a normal distribution.  The
//'        default is normal.
//' @param ncoresRV The number of cores for residual simulation.  By default, this is \code{1}.
//' @param isChol is a boolean indicating that the Cholesky decomposition of the \code{sigma} covariance matrix is supplied instead of
//'        the  \code{sigma} matrix itself.
//' @param amountUnits Dosing amount units.
//' @param timeUnits Time units.
//'
//' @return A data structure to allow C-based for loop (ie solving each
//'       individual in C)
//'
//' @author Matthew L. Fidler
//' @export
//[[Rcpp::export]]
List rxData(const RObject &object,
            const RObject &params = R_NilValue,
            const RObject &events = R_NilValue,
            const Nullable<NumericVector> &inits = R_NilValue,
            const RObject &covs  = R_NilValue,
            const std::string &method = "lsoda",
            const Nullable<LogicalVector> &transit_abs = R_NilValue,
            const double atol = 1.0e-8,
            const double rtol = 1.0e-6,
            const int maxsteps = 5000,
            const int hmin = 0,
            const Nullable<NumericVector> &hmax = R_NilValue,
            const int hini = 0,
            const int maxordn = 12,
            const int maxords = 5,
            const int cores = 1,
            std::string covs_interpolation = "linear",
	    bool addCov = false,
            bool matrix = false,
            const RObject &sigma= R_NilValue,
            const RObject &sigmaDf= R_NilValue,
            const int &sigmaNcores= 1,
            const bool &sigmaIsChol= false,
            const StringVector &amountUnits = NA_STRING,
            const StringVector &timeUnits = "hours"){
  List parData = rxDataParSetup(object,params, events, inits, covs, sigma, sigmaDf,
				sigmaNcores, sigmaIsChol, amountUnits, timeUnits);
  parData["pointer"] = rxSolvingData(object, parData, method, transit_abs, atol,  rtol, maxsteps,
                                     hmin, hmax,  hini, maxordn, maxords, cores, covs_interpolation,
				     addCov, matrix);
  List modVars = rxModelVars(object);
  List ptr = modVars["ptr"];
  parData["get_solve"] = ptr["get_solve"];
  parData["set_solve"] = ptr["set_solve"];
  StringVector cls(3);
  cls(2) = "RxODE.par.data";
  cls(1) = "RxODE.multi.data";
  cls(0) = "RxODE.pointer.multi";
  parData.attr("class") = cls;
  return parData;
}

extern "C"{
  void par_lsoda(SEXP sd);
  void par_dop(SEXP sd);
  rx_solving_options *getRxOp(rx_solve *rx);
  SEXP RxODE_df(SEXP sd);
  SEXP RxODE_par_df(SEXP sd);
}


//[[Rcpp::export]]
SEXP rxSolveC(const RObject &object,
	      const RObject &params = R_NilValue,
	      const RObject &events = R_NilValue,
	      const Nullable<NumericVector> &inits = R_NilValue,
	      const RObject &covs  = R_NilValue,
	      const std::string &method = "lsoda",
	      const Nullable<LogicalVector> &transit_abs = R_NilValue,
	      const double atol = 1.0e-8,
	      const double rtol = 1.0e-6,
	      const int maxsteps = 5000,
	      const int hmin = 0,
	      const Nullable<NumericVector> &hmax = R_NilValue,
	      const int hini = 0,
	      const int maxordn = 12,
	      const int maxords = 5,
	      const int cores = 1,
	      std::string covs_interpolation = "linear",
	      bool addCov = false,
	      bool matrix = false,
	      const RObject &sigma= R_NilValue,
	      const RObject &sigmaDf= R_NilValue,
	      const int &sigmaNcores= 1,
	      const bool &sigmaIsChol= false,
	      const StringVector &amountUnits = NA_STRING,
	      const StringVector &timeUnits = "hours"){
  DataFrame ret;
  List parData = rxData(object, params, events, inits, covs, method, transit_abs, atol,
			rtol, maxsteps, hmin,hmax, hini, maxordn, maxords, cores,
			covs_interpolation, addCov, matrix, sigma, sigmaDf, sigmaNcores, sigmaIsChol,
			amountUnits, timeUnits);
  rx_solve *rx;
  rx =getRxSolve(parData);
  rx_solving_options *op;
  op = getRxOp(rx);
  if (op->stiff == 1){
    // lsoda
    par_lsoda(parData);
  } else if (op->stiff == 0){
    // dop
    par_dop(parData);
  }
  List dat = RxODE_df(parData);
  if (rx->matrix){
    freeRxSolve(parData);
    dat.attr("class") = "data.frame";
    int nr = as<NumericVector>(dat[0]).size();
    NumericMatrix tmpM(nr,dat.size());
    for (int i = 0; i < dat.size(); i++){
      tmpM(_,i) = as<NumericVector>(dat[i]);
    }
    tmpM.attr("dimnames") = List::create(R_NilValue,dat.names());
    return tmpM;
  } else {
    Function newEnv("new.env", R_BaseNamespace);
    Environment RxODE("package:RxODE");
    Environment e = newEnv(_["size"] = 29, _["parent"] = RxODE);
    // Save information
    // Remove one final; Just for debug.
    // e["parData"] = parData;
    List xtra = RxODE_par_df(parData);
    List pd = as<List>(xtra[0]);
    e["params.dat"] = pd;
    if (as<int>(parData["nSub"]) == 1 && as<int>(parData["nsim"]) == 1){
      int n = pd.size();
      NumericVector par2(n);
      for (int i = 0; i <n; i++){
	par2[i] = (as<NumericVector>(pd[i]))[0];
      }
      par2.names() = pd.names();
      e["params.single"] = par2;
    } else {
      e["params.single"] = R_NilValue;
    }
    e["EventTable"] = xtra[1];
    e["dosing"] = xtra[3];
    e["sampling"] = xtra[2];
    e["obs.rec"] = xtra[4];
    e["inits.dat"] = parData["inits"];
    CharacterVector units(2);
    units[0] = as<std::string>(parData["amount.units"]);
    units[1] = as<std::string>(parData["time.units"]);
    CharacterVector unitsN(2);
    unitsN[0] = "dosing";
    unitsN[1] = "time";
    units.names() = unitsN;
    e["units"] = units;
    e["nobs"] = parData["nObs"];
    
    Function eventTable("eventTable",RxODE);
    List et = eventTable(_["amount.units"] = parData["amount.units"], _["time.units"] =parData["time.units"]);
    Function importEt = as<Function>(et["import.EventTable"]);
    importEt(e["EventTable"]);
    e["events.EventTable"] = et;
    freeRxSolve(parData);
    Function parse2("parse", R_BaseNamespace);
    Function eval2("eval", R_BaseNamespace);
    e["get.EventTable"] = eval2(_["expr"]   = parse2(_["text"]="function() EventTable"),
				_["envir"]  = e);
    e["get.dosing"] = eval2(_["expr"]   = parse2(_["text"]="function() dosing"),
			    _["envir"]  = e);
    e["get.sampling"] = eval2(_["expr"]   = parse2(_["text"]="function() sampling"),
                            _["envir"]  = e);
    e["get.obs.rec"] = eval2(_["expr"]   = parse2(_["text"]="function() obs.rec"),
                              _["envir"]  = e);
    e["get.units"] = eval2(_["expr"]   = parse2(_["text"]="function() units"),
			   _["envir"]  = e);
    e["get.nobs"] = eval2(_["expr"]   = parse2(_["text"]="function() nobs"),
                           _["envir"]  = e);
    e["args.object"] =  object;
    e["args.params"] = params;    
    e["args.events"] = events;
    e["args.inits"] = inits;
    e["args.covs"] = covs;
    e["args.method"] = method;
    e["args.transit_abs"] = transit_abs;
    e["args.atol"] = atol;
    e["args.rtol"] = rtol;
    e["args.maxsteps"] = maxsteps;
    e["args.hmin"] = hmin;
    e["args.hmax"] = hmax;
    e["args.hini"] = hini;
    e["args.maxordn"] = maxordn;
    e["args.maxords"] = maxords;
    e["args.cores"] = cores;
    e["args.covs_interpolation"] = covs_interpolation;
    e["args.addCov"] = addCov;
    e["args.matrix"] = matrix;
    e["args.sigma"] = sigma;
    e["args.sigmaDf"] = sigmaDf;
    e["args.sigmaNcores"] = sigmaNcores;
    e["args.sigmaIsChol"] = sigmaIsChol;
    e["args.amountUnits"] = amountUnits;
    e["args.timeUnits"] = timeUnits;
    CharacterVector cls(2);
    cls(0) = "solveRxODE7";
    cls(1) = "data.frame";
    cls.attr(".RxODE.env") = e;    
    dat.attr("class") = cls;
    return(dat);
  }
}

//[[Rcpp::export]]
RObject rxSolveGet(RObject obj,std::string arg){
  if (rxIs(obj, "solveRxODE7")){
    List lst = as<List>(obj);
    CharacterVector nm = lst.names();
    int i = 0, n = nm.size();
    for (i = 0; i < n; i++){
      if (nm[i] == arg){
        return lst[arg];
      }
    }
    CharacterVector cls = lst.attr("class");
    Environment e = as<Environment>(cls.attr(".RxODE.env"));
    if (arg == "env"){
      return as<RObject>(e);
    }
    if (e.exists(arg)){
      return e[arg];
    }
    if (arg == "params" || arg == "par" || arg == "pars" || arg == "param"){
      return rxSolveGet(obj, "params.dat");
    } else if (arg == "inits" || arg == "init"){
      return rxSolveGet(obj, "inits.dat");
    } else if (arg == "t"){
      return rxSolveGet(obj,"time");
    }
    // Now parameters
    List pars = List(e["params.dat"]);
    CharacterVector nmp = pars.names();
    n = pars.size();
    for (i = 0; i < n; i++){
      if (nmp[i] == arg){
        return pars[arg];
      }
    }
    // // Now inis.
    // Function sub("sub", R_BaseNamespace);
    // NumericVector ini = NumericVector(e["inits.dat"]);
    // CharacterVector nmi = ini.names();
    // n = ini.size();
    // std::string cur;
    // for (i = 0; i < n; i++){
    //   cur = as<std::string>(sub("(?:(?:[_.]0)|0|\\(0\\)|\\[0\\]|\\{0\\})$","", arg));
    //   if (nmi[i] == cur){
    //     return wrap(ini[i]);
    //   }
    // }
    // // Sensitivities -- last
    // // This is slower, defer to last.
    // for (i = 0; i < n; i++){
    //   // The regular expression came from rex;  It is a it long...
    //   if (as<std::string>(sub("rx__sens_((?:[a-zA-Z][_a-zA-Z0-9.]*|(?:\\.){1,}[_a-zA-Z][_a-zA-Z0-9.]*))_BY_((?:[a-zA-Z][_a-zA-Z0-9.]*|(?:\\.){1,}[_a-zA-Z][_a-zA-Z0-9.]*))__",
    // 			      "_sens_\\1_\\2", nm[i])) == arg){
    // 	return lst[i];
    //   }
    // }
  }
  return R_NilValue;
}
