%
%   Copyright 2007-2018 by the individuals mentioned in the source code history
%
%   Licensed under the Apache License, Version 2.0 (the "License");
%   you may not use this file except in compliance with the License.
%   You may obtain a copy of the License at
% 
%        http://www.apache.org/licenses/LICENSE-2.0
% 
%   Unless required by applicable law or agreed to in writing, software
%   distributed under the License is distributed on an "AS IS" BASIS,
%   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
%   See the License for the specific language governing permissions and
%   limitations under the License.

\name{mxRun}
\alias{mxRun}

\title{Send a Model to the Optimizer}

\description{
   This function begins optimization, sending \sQuote{model} to the optimizer.

   Optionally, confidence \sQuote{intervals} will also be computed.

   During a run, \sQuote{mxRun} will print the context (e.g. optimizer name or step in the analysis 
   e.g. MxComputeNumericDeriv ), followed by the current evaluation count, fit value, and the change 
   in fit compared to the last status report. e.g.: 
   
   \code{SLSQP 668 4966.58 -218.8}

   note: For models that prove difficult to run, you might look at \code{\link{mxTryHard}}.
}

\usage{
mxRun(model, ..., intervals = NULL, silent = FALSE, suppressWarnings = FALSE, 
    unsafe = FALSE, checkpoint = FALSE, useSocket = FALSE, onlyFrontend = FALSE, 
    useOptimizer = TRUE)
}

\arguments{
    \item{model}{A \link{MxModel} object to be optimized.}    
    \item{...}{Not used.  Forces remaining arguments to be specified by name.}
    \item{intervals}{A boolean indicating whether to compute the specified confidence intervals.}
    \item{silent}{A boolean indicating whether to print status to terminal.}
    \item{suppressWarnings}{A boolean indicating whether to suppress warnings.}    
    \item{unsafe}{A boolean indicating whether to ignore errors.}
    \item{checkpoint}{A boolean indicating whether to periodically write parameter values to a file.}
    \item{useSocket}{A boolean indicating whether to periodically write parameter values to a socket.}
    \item{onlyFrontend}{A boolean indicating whether to run only front-end model transformations.}
    \item{useOptimizer}{A boolean indicating whether to run only the log-likelihood of the current free parameter values but not move any of the free parameters.}
}

\details{
The mxRun function is used to optimize free parameters in \link{MxModel} objects based on an expectation function and fit function. MxModel objects included in the mxRun function must include an appropriate expectation and fit functions.

If the \sQuote{silent} flag is TRUE, then model execution will not print any status messages to the terminal.

If the \sQuote{suppressWarnings} flag is TRUE, then model execution will not issue a warning if NPSOL returns a non-zero status code.

If the \sQuote{unsafe} flag is TRUE, then many error conditions will not
be detected. Any error condition detected will be downgraded to warnings.  It is strongly recommended to use this feature only for debugging purposes.

Free parameters are estimated or updated based on the expectation and fit functions. These estimated values, along with estimation information and model fit, can be found in the 'output' slot of MxModel objects after mxRun has been used.

If a model is dependent on or shares parameters with another model, both models must be included as arguments in another MxModel object. This top-level MxModel object must include expectation and fit functions in both submodels, as well as an additional fit function describing how the results of the first two should be combined (e.g. \code{\link{mxFitFunctionMultigroup}}).
}

\value{
   Returns an MxModel object with free parameters updated to their final values.
   The return value contains an "output" slot with the results of 
   optimization.
}

\seealso{
\link{mxTryHard} for running models which prove difficult to optimize; \link{summary} to print a summary of a run model; \link{mxModel} for more on the model itself; More information about the OpenMx package may be found \link[=OpenMx]{here}. 
}

\references{
The OpenMx User's guide can be found at \url{https://openmx.ssri.psu.edu/documentation}.
}

\examples{
# Create and run the 1-factor CFA on the openmx.ssri.psu.edu front page

# 1. Load OpenMx and the demoOneFactor dataframe

library(OpenMx)
data(demoOneFactor)  

# 2. Define the manifests (5 demo variables) and latents for use in the model

manifests <- names(demoOneFactor) 
latents   <- c("G")

# 3. Build the model, adding paths and data
model <- mxModel(model="One Factor", type="RAM",
    manifestVars = manifests,
    latentVars   = latents,
    mxPath(from=latents, to=manifests, labels=paste("b", 1:5, sep="")),
    mxPath(from=manifests, arrows=2, labels=paste("u", 1:5, sep="")),
    mxPath(from=latents  , arrows=2, free=FALSE, values=1.0),
    mxData(cov(demoOneFactor), type="cov", numObs=500)
)

# 4. Run the model, returning the result into model
model <- mxRun(model) 

# 5. Show a summary of the fitted model and parameter values
summary(model) 

}
