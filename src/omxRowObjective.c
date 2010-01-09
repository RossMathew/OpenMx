/*
 *  Copyright 2007-2010 The OpenMx Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <R.h> 
#include <Rinternals.h>
#include <Rdefines.h>
#include <R_ext/Rdynload.h>
#include <R_ext/BLAS.h>
#include <R_ext/Lapack.h>
#include "omxDefines.h"
#include "omxAlgebraFunctions.h"
#include "omxSymbolTable.h"
#include "omxData.h"

#ifndef _OMX_ROW_OBJECTIVE_
#define _OMX_ROW_OBJECTIVE_ TRUE

typedef struct omxDefinitionVar {		 	// Definition Var

	int data, column;		// Where it comes from
	int numLocations;		// Num locations
	double** location;		// And where it goes
	omxMatrix** matrices;	// Matrix numbers for dirtying

} omxDefinitionVar;

extern void handleDefinitionVarList(omxData* data, int row, omxDefinitionVar* defVars, int numDefs);

typedef struct omxRowObjective {

	/* Parts of the R  MxRowObjective Object */
	omxMatrix* rowAlgebra;		// Row-by-row algebra
	omxMatrix* rowResults;		// Aggregation of row algebra results
	omxMatrix* reduceAlgebra;	// Algebra performed after row-by-row computation
	omxData*   data;				// The data
	omxMatrix* dataColumns;		// The order of columns in the data matrix
	
	/* Structures determined from info in the MxRowObjective Object*/
	omxDefinitionVar* defVars;	// A list of definition variables
	int numDefs;				// The length of the defVars list

} omxRowObjective;

void omxDestroyRowObjective(omxObjective *oo) {

}

omxRListElement* omxSetFinalReturnsRowObjective(omxObjective *oo, int *numReturns) {
	*numReturns = 0;
	omxRListElement* retVal = (omxRListElement*) R_alloc(1, sizeof(omxRListElement));

	retVal[0].numValues = 0;
	
	return retVal;
}


void omxCopyMatrixToRow(omxMatrix* source, int row, omxMatrix* target) {
	
	int i;
	for(i = 0; i < source->cols; source++) {
		omxSetMatrixElement(target, row, i, omxMatrixElement(source, 1, i));
	}
	
}

void omxCallRowObjective(omxObjective *oo) {	// TODO: Figure out how to give access to other per-iteration structures.

	if(OMX_DEBUG) { Rprintf("Beginning Row Evaluation.\n");}
	// Requires: Data, means, covariances.
	// Potential Problem: Definition variables currently are assumed to be at the end of the data matrix.

	int numDefs;

	omxMatrix *rowAlgebra, *rowResults, *reduceAlgebra, *dataColumns;
	omxDefinitionVar* defVars;
	omxData *data;

    omxRowObjective* oro = ((omxRowObjective*)oo->argStruct);
    
	rowAlgebra	  = oro->rowAlgebra;	 // Locals, for readability.  Should compile out.
	rowResults	  = oro->rowResults;
	reduceAlgebra = oro->reduceAlgebra;
	data		= oro->data;
	dataColumns	= oro->dataColumns;
	defVars		= oro->defVars;
	numDefs		= oro->numDefs;
    
	if(numDefs == 0) {
		if(OMX_DEBUG) {Rprintf("Precalculating row algebra for all rows.\n");}
		omxRecompute(rowAlgebra);		// Only recompute this here if there are no definition vars
		if(OMX_DEBUG) { omxPrintMatrix(rowAlgebra, "All Rows Identical:"); }	
		for(int row = 0; row < data->rows; row++) {
			omxCopyMatrixToRow(rowAlgebra, row, rowResults);
		}
	}
	for(int row = 0; row < data->rows; row++) {

		// Handle Definition Variables.
		if(numDefs != 0) {
			if(OMX_DEBUG_ROWS) { Rprintf("Handling definition vars and calculating algebra for row %d.\n", row); }
			handleDefinitionVarList(data, row, defVars, numDefs);
			omxStateNextRow(oo->matrix->currentState);						// Advance row
			omxRecompute(rowAlgebra);										// Calculate this row
		}
		omxCopyMatrixToRow(rowAlgebra, row, rowResults);
	}
	
	omxRecompute(reduceAlgebra);

	omxCopyMatrix(oo->matrix, reduceAlgebra);

}

unsigned short int omxNeedsUpdateRowObjective(omxObjective* oo) {
	return omxMatrixNeedsUpdate(((omxRowObjective*)oo->argStruct)->rowAlgebra)
		|| omxMatrixNeedsUpdate(((omxRowObjective*)oo->argStruct)->reduceAlgebra);
}

void omxInitRowObjective(omxObjective* oo, SEXP rObj) {

	if(OMX_DEBUG) { Rprintf("Initializing Row/Reduce objective function.\n"); }

	SEXP nextMatrix, itemList, nextItem, dataSource, columnSource;
	int nextDef, index;
	int *nextInt;
	omxRowObjective *newObj = (omxRowObjective*) R_alloc(1, sizeof(omxRowObjective));

	if(OMX_DEBUG) {Rprintf("Accessing data source.\n"); }
	PROTECT(nextMatrix = GET_SLOT(rObj, install("data"))); 
	newObj->data = omxNewDataFromMxDataPtr(nextMatrix, oo->matrix->currentState);
	if(newObj->data == NULL) { 
		char errstr[250];
		sprintf(errstr, "No data provided to omxRowObjective.");
		omxRaiseError(oo->matrix->currentState, -1, errstr);
	}
	UNPROTECT(1); // nextMatrix

	PROTECT(nextMatrix = GET_SLOT(rObj, install("rowAlgebra")));
	newObj->rowAlgebra = omxNewMatrixFromMxIndex(nextMatrix, oo->matrix->currentState);
	if(newObj->rowAlgebra == NULL) { 
		char errstr[250];
		sprintf(errstr, "No row-wise algebra in omxRowObjective.");
		omxRaiseError(oo->matrix->currentState, -1, errstr);
	}
	UNPROTECT(1);// nextMatrix

	PROTECT(nextMatrix = GET_SLOT(rObj, install("rowResults")));
	newObj->rowResults = omxNewMatrixFromMxIndex(nextMatrix, oo->matrix->currentState);
	if(newObj->rowResults == NULL) { 
		char errstr[250];
		sprintf(errstr, "No row results matrix in omxRowObjective.");
		omxRaiseError(oo->matrix->currentState, -1, errstr);
	}
	UNPROTECT(1);// nextMatrix
	
	PROTECT(nextMatrix = GET_SLOT(rObj, install("reduceAlgebra")));
	newObj->reduceAlgebra = omxNewMatrixFromMxIndex(nextMatrix, oo->matrix->currentState);
	if(newObj->reduceAlgebra == NULL) { 
		char errstr[250];
		sprintf(errstr, "No row reduction algebra in omxRowObjective.");
		omxRaiseError(oo->matrix->currentState, -1, errstr);
	}
	UNPROTECT(1);// nextMatrix
	
	if(OMX_DEBUG) {Rprintf("Accessing variable mapping structure.\n"); }
	PROTECT(nextMatrix = GET_SLOT(rObj, install("dataColumns")));
	newObj->dataColumns = omxNewMatrixFromMxMatrix(nextMatrix, oo->matrix->currentState);
	if(OMX_DEBUG) {omxPrint(newObj->dataColumns, "Variable mapping"); }
	UNPROTECT(1);
	
	if(OMX_DEBUG) {Rprintf("Accessing definition variables structure.\n"); }
	PROTECT(nextMatrix = GET_SLOT(rObj, install("definitionVars")));
	newObj->numDefs = length(nextMatrix);
	if(OMX_DEBUG) {Rprintf("Number of definition variables is %d.\n", newObj->numDefs); }
	newObj->defVars = (omxDefinitionVar *) R_alloc(newObj->numDefs, sizeof(omxDefinitionVar));
	for(nextDef = 0; nextDef < newObj->numDefs; nextDef++) {
		PROTECT(itemList = VECTOR_ELT(nextMatrix, nextDef));
		PROTECT(dataSource = VECTOR_ELT(itemList, 0));
		if(OMX_DEBUG) {Rprintf("Data source number is %d.\n", INTEGER(dataSource)[0]); }
		newObj->defVars[nextDef].data = INTEGER(dataSource)[0];
		PROTECT(columnSource = VECTOR_ELT(itemList, 1));
		if(OMX_DEBUG) {Rprintf("Data column number is %d.\n", INTEGER(columnSource)[0]); }
		newObj->defVars[nextDef].column = INTEGER(columnSource)[0];
		UNPROTECT(2); // unprotect dataSource and columnSource
		newObj->defVars[nextDef].numLocations = length(itemList) - 2;
		newObj->defVars[nextDef].location = (double **) R_alloc(length(itemList) - 2, sizeof(double*));
		newObj->defVars[nextDef].matrices = (omxMatrix **) R_alloc(length(itemList) - 2, sizeof(omxMatrix*));
		for(index = 2; index < length(itemList); index++) {
			PROTECT(nextItem = VECTOR_ELT(itemList, index));
			newObj->defVars[nextDef].location[index-2] = omxLocationOfMatrixElement(
				oo->matrix->currentState->matrixList[INTEGER(nextItem)[0]],
				INTEGER(nextItem)[1], INTEGER(nextItem)[2]);
			newObj->defVars[nextDef].matrices[index-2] = oo->matrix->currentState->matrixList[INTEGER(nextItem)[0]];
			UNPROTECT(1); // unprotect nextItem
		}
		UNPROTECT(1); // unprotect itemList
	}
	UNPROTECT(1); // unprotect nextMatrix

	oo->objectiveFun = omxCallRowObjective;
	oo->needsUpdateFun = omxNeedsUpdateRowObjective;
	oo->setFinalReturns = omxSetFinalReturnsRowObjective;
	oo->destructFun = omxDestroyRowObjective;
	oo->repopulateFun = NULL;

	oo->argStruct = (void*) newObj;
}

#endif /* _OMX_ROW_OBJECTIVE_ */
