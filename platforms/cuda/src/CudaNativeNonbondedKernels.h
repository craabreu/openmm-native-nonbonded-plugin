#ifndef CUDA_NATIVENONBONDED_KERNELS_H_
#define CUDA_NATIVENONBONDED_KERNELS_H_

/* -------------------------------------------------------------------------- *
 *                                   OpenMM                                   *
 * -------------------------------------------------------------------------- *
 * This is part of the OpenMM molecular simulation toolkit originating from   *
 * Simbios, the NIH National Center for Physics-Based Simulation of           *
 * Biological Structures at Stanford, funded under the NIH Roadmap for        *
 * Medical Research, grant U54 GM072970. See https://simtk.org.               *
 *                                                                            *
 * Portions copyright (c) 2016 Stanford University and the Authors.           *
 * Authors: Peter Eastman                                                     *
 * Contributors:                                                              *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,    *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR      *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE  *
 * USE OR OTHER DEALINGS IN THE SOFTWARE.                                     *
 * -------------------------------------------------------------------------- */

#include "NativeNonbondedKernels.h"
#include "openmm/internal/ContextImpl.h"
#include "openmm/cuda/CudaContext.h"
#include "openmm/cuda/CudaArray.h"
#include "openmm/cuda/CudaSort.h"
#include "openmm/cuda/CudaFFT3D.h"
#include <vector>
#include <cufft.h>

using namespace OpenMM;
using namespace std;

namespace NativeNonbondedPlugin {

/**
 * This kernel is invoked by NativeNonbondedForce to calculate the forces acting on the system.
 */
class CudaCalcNativeNonbondedForceKernel : public CalcNativeNonbondedForceKernel {
public:
    CudaCalcNativeNonbondedForceKernel(std::string name, const Platform& platform, CudaContext& cu, const System& system) : CalcNativeNonbondedForceKernel(name, platform),
            cu(cu), hasInitializedFFT(false), sort(NULL), dispersionFft(NULL), fft(NULL), pmeio(NULL), usePmeStream(false) {
    }
    ~CudaCalcNativeNonbondedForceKernel();
    /**
     * Initialize the kernel.
     *
     * @param system     the System this kernel will be applied to
     * @param force      the NativeNonbondedForce this kernel will be used for
     */
    void initialize(const System& system, const NativeNonbondedForce& force);
    /**
     * Execute the kernel to calculate the forces and/or energy.
     *
     * @param context        the context in which to execute this kernel
     * @param includeForces  true if forces should be calculated
     * @param includeEnergy  true if the energy should be calculated
     * @param includeDirect  true if direct space interactions should be included
     * @param includeReciprocal  true if reciprocal space interactions should be included
     * @return the potential energy due to the force
     */
    double execute(ContextImpl& context, bool includeForces, bool includeEnergy, bool includeDirect, bool includeReciprocal);
    /**
     * Copy changed parameters over to a context.
     *
     * @param context    the context to copy parameters to
     * @param force      the NativeNonbondedForce to copy the parameters from
     */
    void copyParametersToContext(ContextImpl& context, const NativeNonbondedForce& force);
    /**
     * Get the parameters being used for PME.
     * 
     * @param alpha   the separation parameter
     * @param nx      the number of grid points along the X axis
     * @param ny      the number of grid points along the Y axis
     * @param nz      the number of grid points along the Z axis
     */
    void getPMEParameters(double& alpha, int& nx, int& ny, int& nz) const;
    /**
     * Get the dispersion parameters being used for the dispersion term in LJPME.
     * 
     * @param alpha   the separation parameter
     * @param nx      the number of grid points along the X axis
     * @param ny      the number of grid points along the Y axis
     * @param nz      the number of grid points along the Z axis
     */
    void getLJPMEParameters(double& alpha, int& nx, int& ny, int& nz) const;
private:
    class SortTrait : public CudaSort::SortTrait {
        int getDataSize() const {return 8;}
        int getKeySize() const {return 4;}
        const char* getDataType() const {return "int2";}
        const char* getKeyType() const {return "int";}
        const char* getMinKey() const {return "(-2147483647-1)";}
        const char* getMaxKey() const {return "2147483647";}
        const char* getMaxValue() const {return "make_int2(2147483647, 2147483647)";}
        const char* getSortKey() const {return "value.y";}
    };
    class ForceInfo;
    class PmeIO;
    class PmePreComputation;
    class PmePostComputation;
    class SyncStreamPreComputation;
    class SyncStreamPostComputation;
    CudaContext& cu;
    ForceInfo* info;
    bool hasInitializedFFT;
    CudaArray charges;
    CudaArray sigmaEpsilon;
    CudaArray exceptionParams;
    CudaArray exclusionAtoms;
    CudaArray exclusionParams;
    CudaArray baseParticleParams;
    CudaArray baseExceptionParams;
    CudaArray particleParamOffsets;
    CudaArray exceptionParamOffsets;
    CudaArray particleOffsetIndices;
    CudaArray exceptionOffsetIndices;
    CudaArray globalParams;
    CudaArray cosSinSums;
    CudaArray pmeGrid1;
    CudaArray pmeGrid2;
    CudaArray pmeBsplineModuliX;
    CudaArray pmeBsplineModuliY;
    CudaArray pmeBsplineModuliZ;
    CudaArray pmeDispersionBsplineModuliX;
    CudaArray pmeDispersionBsplineModuliY;
    CudaArray pmeDispersionBsplineModuliZ;
    CudaArray pmeAtomGridIndex;
    CudaArray pmeEnergyBuffer;
    CudaSort* sort;
    Kernel cpuPme;
    PmeIO* pmeio;
    CUstream pmeStream;
    CUevent pmeSyncEvent, paramsSyncEvent;
    CudaFFT3D* fft;
    cufftHandle fftForward;
    cufftHandle fftBackward;
    CudaFFT3D* dispersionFft;
    cufftHandle dispersionFftForward;
    cufftHandle dispersionFftBackward;
    CUfunction computeParamsKernel, computeExclusionParamsKernel;
    CUfunction ewaldSumsKernel;
    CUfunction ewaldForcesKernel;
    CUfunction pmeGridIndexKernel;
    CUfunction pmeDispersionGridIndexKernel;
    CUfunction pmeSpreadChargeKernel;
    CUfunction pmeDispersionSpreadChargeKernel;
    CUfunction pmeFinishSpreadChargeKernel;
    CUfunction pmeDispersionFinishSpreadChargeKernel;
    CUfunction pmeEvalEnergyKernel;
    CUfunction pmeEvalDispersionEnergyKernel;
    CUfunction pmeConvolutionKernel;
    CUfunction pmeDispersionConvolutionKernel;
    CUfunction pmeInterpolateForceKernel;
    CUfunction pmeInterpolateDispersionForceKernel;
    std::vector<std::pair<int, int> > exceptionAtoms;
    std::vector<std::string> paramNames;
    std::vector<double> paramValues;
    double ewaldSelfEnergy, dispersionCoefficient, alpha, dispersionAlpha;
    int interpolateForceThreads;
    int gridSizeX, gridSizeY, gridSizeZ;
    int dispersionGridSizeX, dispersionGridSizeY, dispersionGridSizeZ;
    bool hasCoulomb, hasLJ, usePmeStream, useCudaFFT, doLJPME, usePosqCharges, recomputeParams, hasOffsets;
    NonbondedMethod nonbondedMethod;
    static const int PmeOrder = 5;
};

} // namespace NativeNonbondedPlugin

#endif /*CUDA_NATIVENONBONDED_KERNELS_H_*/
