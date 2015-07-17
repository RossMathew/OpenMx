#
#   Copyright 2007-2015 The OpenMx Project
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

#------------------------------------------------------------------------------
# Author:
# Date: 2015-06-14
# Filename: MxFactorScores.R
# Purpose: Write a helper function for computing various type of factor scores
#------------------------------------------------------------------------------


mxFactorScores <- function(model, type=c('ML', 'WeightedML', 'Regression')){
	if(model$data$type!='raw'){
		stop("The 'model' arugment must have raw data.")
	}
	if(!(class(model$expectation) %in% "MxExpectationLISREL")){
		stop('Factor scores are only implemented for LISREL expectations.')
	}
	lx <- mxEvalByName(model$expectation$LX, model, compute=TRUE)
	nksix <- dim(lx)
	nksi <- nksix[2]
	nx <- nksix[1]
	nrows <- nrow(model$data$observed)
	res <- array(NA, c(nrows, nksi, 2))
	if(any(type %in% c('ML', 'WeightedML'))){
		model <- omxSetParameters(model, labels=names(omxGetParameters(model)), free=FALSE)
		ksiMean <- mxEvalByName(model$expectation$KA, model, compute=TRUE)
		newKappa <- mxMatrix("Full", nksi, 1, values=ksiMean, free=TRUE, name="Score", labels=paste0("fscore", 1:nksi))
		scoreKappa <- mxAlgebraFromString(paste("Score -", model$expectation$KA), name="SKAPPA", dimnames=list(dimnames(lx)[[2]], dimnames(lx)[[2]]))
		newExpect <- mxExpectationLISREL(LX=model$expectation$LX, PH=model$expectation$PH, TD=model$expectation$TD, TX=model$expectation$TX, KA="SKAPPA", thresholds=model$expectation$thresholds)
		newWeight <- mxAlgebraFromString(paste0("log(det(", model$expectation$PH, ")) + ( (t(SKAPPA)) %&% ", model$expectation$PH, " ) + ", nksi, "*log(2*3.1415926535)"), name="weight")
		work <- mxModel(model=model, name=paste("FactorScores", model$name, sep=''), newKappa, scoreKappa, newExpect, newWeight)
		if(type[1]=='WeightedML'){
			wup <- mxModel(model="Container", work,
				mxAlgebraFromString(paste(work@name, ".weight + ", work@name, ".fitfunction", sep=""), name="wtf"),
				mxFitFunctionAlgebra("wtf")
			)
			work <- wup
		}
		work@data <- NULL
		for(i in 1:nrows){
			if(type[1]=='ML'){
				fit <- mxModel(model=work, name=paste0(work@name, i, "Of", nrows), mxData(model$data$observed[i,,drop=FALSE], 'raw'))
			} else if(type[1]=='WeightedML'){
				work@submodels[[1]]@data <- mxData(model$data$observed[i,,drop=FALSE], 'raw')
				fit <- mxModel(model=work, name=paste0(work@name, i, "Of", nrows))
			}
			fit <- mxRun(fit, silent=!as.logical(i%%100), suppressWarnings=TRUE)
			res[i,,1] <- omxGetParameters(fit) #params
			res[i,,2] <- fit$output$standardErrors #SEs
		}
	} else if(type=='Regression'){
		if(!single.na(model$expectation$thresholds)){
			stop('Regression factor scores cannot be computed when there are thresholds (ordinal data).')
		}
		ss <- mxModel(model=model,
			mxMatrix('Zero', nksi, nksi, name='stateSpaceA'),
			mxMatrix('Zero', nksi, nx, name='stateSpaceB'),
			mxMatrix('Iden', nx, nx, name='stateSpaceD'),
			mxMatrix('Iden', nksi, nksi, name='stateSpaceP0'),
			mxExpectationStateSpace(A='stateSpaceA', B='stateSpaceB', C=model$expectation$LX, D='stateSpaceD', Q=model$expectation$PH, R=model$expectation$TD, x0=model$expectation$KA, P0='stateSpaceP0', u=model$expectation$TX))
		resDel <- mxKalmanScores(ss)
		res[,,1] <- resDel$xUpdated[-1,, drop=FALSE]
		res[,,2] <- apply(resDel$PUpdated[,,-1, drop=FALSE], 3, function(x){sqrt(diag(x))})
	} else {
		stop('Unknown type argument to mxFactorScores')
	}
	return(res)
}



