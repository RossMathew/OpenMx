/*
 *  Copyright 2007-2018 by the individuals mentioned in the source code history
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

#include "omxFitFunction.h"
#include "matrix.h"
#include "omxBuffer.h"
#include <algorithm>
#include "Compute.h"
#include "EnableWarnings.h"

struct AlgebraFitFunction : omxFitFunction {
	omxFitFunction *ff;
	omxMatrix *algebra;
	omxMatrix *gradient;
	omxMatrix *hessian;
	int verbose;

	FreeVarGroup *varGroup;
	int numDeriv;
	std::vector<int> gradMap;
	bool vec2diag;

	AlgebraFitFunction() : ff(0), gradient(0), hessian(0), verbose(false), varGroup(0) {};
	virtual void init();
	virtual void compute(int ffcompute, FitContext *fc);
	void setVarGroup(FreeVarGroup *);
};

void AlgebraFitFunction::setVarGroup(FreeVarGroup *vg)
{
	varGroup = vg;

	if (verbose) {
		mxLog("%s: rebuild parameter map for var group %d",
		      ff->matrix->name(), varGroup->id[0]);
	}
	numDeriv = 0;

	if (gradient) {
		if (int(std::max(gradient->rownames.size(),
				 gradient->colnames.size())) !=
		    std::max(gradient->rows, gradient->cols)) {
			Rf_error("%s: gradient must have row or column names", ff->matrix->name());
		}
	}
	if (hessian) {
		if (hessian->rows != hessian->cols) {
			Rf_error("%s: Hessian must be square (instead of %dx%d)",
				 ff->matrix->name(), hessian->rows, hessian->cols);
		}
		if (int(hessian->rownames.size()) != hessian->rows ||
		    int(hessian->colnames.size()) != hessian->rows) {
			Rf_error("%s: Hessian must have row and column names", ff->matrix->name());
		}
		for (int hx=0; hx < hessian->rows; ++hx) {
			if (strcmp(hessian->colnames[hx], hessian->rownames[hx]) != 0) {
				Rf_error("%s: Hessian must have identical row and column names (mismatch at %d)",
					 ff->matrix->name(), 1+hx);
			}
		}
		// Is this the best way to test? TODO
		vec2diag = hessian->algebra->oate && strcmp(hessian->algebra->oate->rName, "vec2diag")==0;
	}
	if (gradient && hessian) {
		int size = gradient->rows * gradient->cols;
		if (hessian->rows != size) {
			Rf_error("%s: derivatives non-conformable (gradient is size %d and Hessian is %dx%d)",
				 ff->matrix->name(), size, hessian->rows, hessian->cols);
		}
		std::vector<const char*> &gnames = gradient->rownames;
		if (gnames.size() == 0) gnames = gradient->colnames;
		for (int hx=0; hx < hessian->rows; ++hx) {
			if (strcmp(hessian->colnames[hx], gnames[hx]) != 0) {
				Rf_error("%s: Hessian and gradient must have identical names (mismatch at %d)",
					 ff->matrix->name(), 1+hx);
			}
		}
	}

	std::vector<const char*> *names = NULL;
	if (gradient) {
		names = &gradient->rownames;
		if (names->size() == 0) names = &gradient->colnames;
	}
	if (hessian && names == NULL) {
		names = &hessian->rownames;
	}
	if (!names) return;

	gradMap.resize(names->size());
	for (size_t nx=0; nx < names->size(); ++nx) {
		int to = varGroup->lookupVar((*names)[nx]);
		gradMap[nx] = to;
		if (to >= 0) ++numDeriv;
		if (verbose) {
			mxLog("%s: name '%s' mapped to free parameter %d",
			      ff->matrix->name(), (*names)[nx], gradMap[nx]);
		}
	}
}

// writes to upper triangle of full matrix
static void addSymOuterProd(const double weight, const double *vec, const int len, double *out)
{
	for (int d1=0; d1 < len; ++d1) {
		for (int d2=0; d2 <= d1; ++d2) {
			out[d1 * len + d2] += weight * vec[d1] * vec[d2];
		}
	}
}

void AlgebraFitFunction::compute(int want, FitContext *fc)
{
	if (fc && varGroup != fc->varGroup) {
		setVarGroup(fc->varGroup);
	}

	if (want & (FF_COMPUTE_FIT | FF_COMPUTE_INITIAL_FIT | FF_COMPUTE_PREOPTIMIZE)) {
		if (algebra) {
			omxRecompute(algebra, fc);
			ff->matrix->data[0] = algebra->data[0];
		} else {
			ff->matrix->data[0] = 0;
		}
	}

	if (gradMap.size() == 0) return;
	if (gradient) {
		omxRecompute(gradient, fc);
		if (want & FF_COMPUTE_GRADIENT) {
			for (size_t v1=0; v1 < gradMap.size(); ++v1) {
				int to = gradMap[v1];
				if (to < 0) continue;
				fc->grad(to) += omxVectorElement(gradient, v1);
			}
		}
		if (want & FF_COMPUTE_INFO && fc->infoMethod == INFO_METHOD_MEAT) {
			std::vector<double> grad(varGroup->vars.size());
			for (size_t v1=0; v1 < gradMap.size(); ++v1) {
				int to = gradMap[v1];
				if (to < 0) continue;
				grad[to] += omxVectorElement(gradient, v1);
			}
			addSymOuterProd(1, grad.data(), varGroup->vars.size(), fc->infoB);
		}
	}
	if (hessian && ((want & (FF_COMPUTE_HESSIAN | FF_COMPUTE_IHESSIAN)) ||
			(want & FF_COMPUTE_INFO && fc->infoMethod == INFO_METHOD_HESSIAN))) {
		omxRecompute(hessian, fc);

		if (!vec2diag) {
			HessianBlock *hb = new HessianBlock;
			hb->vars.resize(numDeriv);
			int vx=0;
			for (size_t h1=0; h1 < gradMap.size(); ++h1) {
				if (gradMap[h1] < 0) continue;
				hb->vars[vx] = gradMap[h1];
				++vx;
			}
			hb->mat.resize(numDeriv, numDeriv);
			for (size_t d1=0, h1=0; h1 < gradMap.size(); ++h1) {
				if (gradMap[h1] < 0) continue;
				for (size_t d2=0, h2=0; h2 <= h1; ++h2) {
					if (gradMap[h2] < 0) continue;
					if (h1 == h2) {
						hb->mat(d2,d1) = omxMatrixElement(hessian, h2, h1);
					} else {
						double coef1 = omxMatrixElement(hessian, h2, h1);
						double coef2 = omxMatrixElement(hessian, h1, h2);
						if (coef1 != coef2) {
							Rf_warning("%s: Hessian algebra '%s' is not symmetric at [%d,%d]",
								   ff->matrix->name(), hessian->name(), 1+h2, 1+h1);
						}
						hb->mat(d2,d1) = coef1;
					}
					++d2;
				}
				++d1;
			}
			fc->queue(hb);
		} else {
			for (size_t h1=0; h1 < gradMap.size(); ++h1) {
				int to = gradMap[h1];
				if (to < 0) continue;
				HessianBlock *hb = new HessianBlock;
				hb->vars.assign(1, to);
				hb->mat.resize(1,1);
				hb->mat(0,0) = omxMatrixElement(hessian, h1, h1);
				fc->queue(hb);
			}
		}
	}
	// complain if unimplemented FF_COMPUTE_INFO requested? TODO
}

omxFitFunction *omxInitAlgebraFitFunction()
{ return new AlgebraFitFunction; }

void AlgebraFitFunction::init()
{
	auto *off = this;
	omxState *currentState = off->matrix->currentState;
	
	AlgebraFitFunction *aff = this;
	aff->ff = off;

	ProtectedSEXP Ralg(R_do_slot(rObj, Rf_install("algebra")));
	aff->algebra = omxMatrixLookupFromState1(Ralg, currentState);

	ProtectedSEXP Runit(R_do_slot(rObj, Rf_install("units")));
	off->setUnitsFromName(CHAR(STRING_ELT(Runit, 0)));

	ProtectedSEXP Rgr(R_do_slot(rObj, Rf_install("gradient")));
	aff->gradient = omxMatrixLookupFromState1(Rgr, currentState);
	if (aff->gradient) off->gradientAvailable = TRUE;

	ProtectedSEXP Rh(R_do_slot(rObj, Rf_install("hessian")));
	aff->hessian = omxMatrixLookupFromState1(Rh, currentState);
	if (aff->hessian) off->hessianAvailable = TRUE;

	ProtectedSEXP Rverb(R_do_slot(rObj, Rf_install("verbose")));
	aff->verbose = Rf_asInteger(Rverb);
	off->canDuplicate = true;
}
