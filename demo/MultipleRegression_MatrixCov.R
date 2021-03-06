#
#   Copyright 2007-2018 by the individuals mentioned in the source code history
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

# -----------------------------------------------------------------------
# Program: MultipleRegression_MatrixCov.R  
# Author: Ryne Estabrook
# Date: 2009.08.01 
#
# ModelType: Regression
# DataType: Continuous
# Field: None
#
# Purpose: 
#      Multiple Regression model to estimate effect of independent 
#      on dependent variables
#      Matrix style model input - Covariance matrix data input
#
# RevisionHistory:
#      Hermine Maes -- 2009.10.08 updated & reformatted
#      Ross Gore -- 2011.06.15 added Model, Data & Field
#      Hermine Maes -- 2014.11.02 piecewise specification
# -----------------------------------------------------------------------------

require(OpenMx)
# Load Libraries
# -----------------------------------------------------------------------------

myRegDataCov <- matrix(
    c(0.808,-0.110, 0.089, 0.361,
     -0.110, 1.116, 0.539, 0.289,
      0.089, 0.539, 0.933, 0.312,
      0.361, 0.289, 0.312, 0.836),
    nrow=4,
    dimnames=list(
      c("w","x","y","z"),
      c("w","x","y","z"))
)

 
myRegDataMeans <- c(2.582, 0.054, 2.574, 4.061)
names(myRegDataMeans) <- c("w","x","y","z") 

MultipleDataCov <- myRegDataCov[c("x","y","z"),c("x","y","z")]	
MultipleDataMeans <- myRegDataMeans[c(2,3,4)]
# Prepare Data
# -----------------------------------------------------------------------------

dataCov     <- mxData( observed=MultipleDataCov, type="cov", numObs=100, 
      					 mean=MultipleDataMeans )
matrA       <- mxMatrix( type="Full", nrow=3, ncol=3,
                         free=  c(F,F,F,  T,F,T,  F,F,F),
                         values=c(0,0,0,  1,0,1,  0,0,0),
                         labels=c(NA,NA,NA, "betax",NA,"betaz", NA,NA,NA),
                         byrow=TRUE, name="A" )
matrS       <- mxMatrix( type="Symm", nrow=3, ncol=3, 
                         free=c(T,F,T,  F,T,F,  T,F,T),
                         values=c(1,0,.5,  0,1,0,  .5,0,1),
                         labels=c("varx",NA,"covxz", NA,"residual",NA, "covxz",NA,"varz"),
                         byrow=TRUE, name="S" )
matrF       <- mxMatrix( type="Iden", nrow=3, ncol=3, name="F" )
matrM       <- mxMatrix( type="Full", nrow=1, ncol=3, 
                         free=c(T,T,T), values=c(0,0,0),
                         labels=c("meanx","beta0","meanz"), name="M" )
exp         <- mxExpectationRAM("A","S","F","M", dimnames=c("x","y","z") )
funML       <- mxFitFunctionML()
multiRegModel <- mxModel("Multiple Regression Matrix Specification", 
                         dataCov, matrA, matrS, matrF, matrM, exp, funML)

# Create an MxModel object
# -----------------------------------------------------------------------------
      
multiRegFit <- mxRun(multiRegModel)

summary(multiRegFit)
multiRegFit$output


omxCheckCloseEnough(coef(multiRegFit)[["beta0"]], 1.6312, 0.001)
omxCheckCloseEnough(coef(multiRegFit)[["betax"]], 0.4243, 0.001)
omxCheckCloseEnough(coef(multiRegFit)[["betaz"]], 0.2265, 0.001)
omxCheckCloseEnough(coef(multiRegFit)[["residual"]], 0.6272, 0.001)
omxCheckCloseEnough(coef(multiRegFit)[["varx"]], 1.1040, 0.001)
omxCheckCloseEnough(coef(multiRegFit)[["varz"]], 0.8276, 0.001)
omxCheckCloseEnough(coef(multiRegFit)[["covxz"]], 0.2861, 0.001)
omxCheckCloseEnough(coef(multiRegFit)[["meanx"]], 0.0540, 0.001)
omxCheckCloseEnough(coef(multiRegFit)[["meanz"]], 4.0610, 0.001)
# Compare OpenMx results to Mx results 
# -----------------------------------------------------------------------------
