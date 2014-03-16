/*
 *  Copyright 2007-2014 The OpenMx Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <sys/stat.h>

#define R_NO_REMAP
#include <R.h>
#include <Rinternals.h>

#include "omxDefines.h"
#include "omxState.h"
#include "omxNPSOLSpecific.h"
#include "glue.h"
#include "omxExportBackendState.h"

void omxExportResults(omxState *currentState, MxRList *out)
{
	SEXP matrices;
	SEXP algebras;
	SEXP expectations;

	Rf_protect(matrices = Rf_allocVector(VECSXP, globalState->matrixList.size()));
	Rf_protect(algebras = Rf_allocVector(VECSXP, globalState->algebraList.size()));
	Rf_protect(expectations = Rf_allocVector(VECSXP, globalState->expectationList.size()));

	SEXP nextMat, algebra;
	for(size_t index = 0; index < currentState->matrixList.size(); index++) {
		if(OMX_DEBUG) { mxLog("Final Calculation and Copy of Matrix %lu.", index); }
		omxMatrix* nextMatrix = currentState->matrixList[index];
		omxRecompute(nextMatrix);
		nextMat = omxExportMatrix(nextMatrix);
		SET_VECTOR_ELT(matrices, index, nextMat);
	}

	for(size_t index = 0; index < currentState->algebraList.size(); index++) {
		if(OMX_DEBUG) { mxLog("Final Calculation and Copy of Algebra %lu.", index); }
		omxMatrix* nextAlgebra = currentState->algebraList[index];
		omxInitialCompute(nextAlgebra);
		algebra = omxExportMatrix(nextAlgebra);
		/* If an fit function, populate attributes.  Will skip if not fit function. */
		omxFitFunction* currentFit = nextAlgebra->fitFunction;
		if(currentFit != NULL) {
			if(OMX_DEBUG) { mxLog("Algebra %lu is a fit function.", index); }
			if(currentFit->populateAttrFun != NULL) {
				if(OMX_DEBUG) { mxLog("Algebra %lu has attribute population.", index); }
				currentFit->populateAttrFun(currentFit, algebra);
		    }
		}

		if(OMX_DEBUG) { mxLog("Final Calculation of Algebra %lu Complete.", index); }
		SET_VECTOR_ELT(algebras, index, algebra);
	}
	if(OMX_DEBUG) { mxLog("All Algebras complete."); }
	
	for(size_t index = 0; index < currentState->expectationList.size(); index++) {
		if(OMX_DEBUG) { mxLog("Final Calculation of Expectation %lu.", index); }
		omxExpectation* nextExpectation = currentState->expectationList[index];
		omxExpectationRecompute(nextExpectation);
		SEXP rExpect;
		Rf_protect(rExpect = Rf_allocVector(LGLSXP, 1)); // placeholder to attach attributes
		if(nextExpectation->populateAttrFun != NULL) {
			if(OMX_DEBUG) { mxLog("Expectation %lu has attribute population.", index); }
			nextExpectation->populateAttrFun(nextExpectation, rExpect);
	    }
		SET_VECTOR_ELT(expectations, index, rExpect);
	}

	out->add("matrices", matrices);
	out->add("algebras", algebras);
	out->add("expectations", expectations);
}

void omxPopulateFitFunction(omxMatrix *om, MxRList *result) // deprecated
{
	omxFitFunction* off = om->fitFunction;
	if (!off) return;

	off->addOutput(off, result);

	if (off->setFinalReturns == NULL) return;

	int numEls;
	SEXP oElement;
	omxRListElement* orle = off->setFinalReturns(off, &numEls);
	if (!orle || numEls == 0) return;

	if(OMX_DEBUG) { mxLog("Adding %d sets of fit function Info....", numEls);}
	for(int i = 0; i < numEls; i++) {
		if (!orle[i].values) {
			Rf_warning("Ignored %s in omxPopulateFitFunction", orle[i].label);
			continue;
		}
		if (orle[i].numValues == -1) {
			Rf_protect(oElement = Rf_allocMatrix(REALSXP, orle[i].rows, orle[i].cols));
		} else {
			Rf_protect(oElement = Rf_allocVector(REALSXP, orle[i].numValues));
		}
		memcpy(REAL(oElement), orle[i].values, sizeof(double)*LENGTH(oElement)); // TODO avoid another copy
		result->add(orle[i].label, oElement);
	}
}

void omxPopulateConfidenceIntervals(SEXP intervals, SEXP intervalCodes) {
	int numInts = Global->numIntervals;
	if(OMX_DEBUG) { mxLog("Populating CIs for %d fit functions.", numInts); }
	double* interval = REAL(intervals);
	int* intervalCode = INTEGER(intervalCodes);
	for(int j = 0; j < numInts; j++) {
		omxConfidenceInterval *oCI = Global->intervalList + j;
		interval[j] = oCI->min;
		interval[j + numInts] = oCI->max;
		intervalCode[j] = oCI->lCode;
		intervalCode[j + numInts] = oCI->uCode;
	}
}
