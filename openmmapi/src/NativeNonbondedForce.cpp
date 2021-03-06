/* -------------------------------------------------------------------------- *
 *                                   OpenMM                                   *
 * -------------------------------------------------------------------------- *
 * This is part of the OpenMM molecular simulation toolkit originating from   *
 * Simbios, the NIH National Center for Physics-Based Simulation of           *
 * Biological Structures at Stanford, funded under the NIH Roadmap for        *
 * Medical Research, grant U54 GM072970. See https://simtk.org.               *
 *                                                                            *
 * Portions copyright (c) 2008-2021 Stanford University and the Authors.      *
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

#include "NativeNonbondedForce.h"
#include "internal/NativeNonbondedForceImpl.h"
#include "openmm/Force.h"
#include "openmm/OpenMMException.h"
#include "openmm/internal/AssertionUtilities.h"
#include <cmath>
#include <map>
#include <sstream>
#include <utility>

using namespace NativeNonbondedPlugin;
using namespace OpenMM;
using std::map;
using std::pair;
using std::set;
using std::string;
using std::stringstream;
using std::vector;

NativeNonbondedForce::NativeNonbondedForce() : nonbondedMethod(NoCutoff), cutoffDistance(1.0), switchingDistance(-1.0), rfDielectric(78.3),
        ewaldErrorTol(5e-4), alpha(0.0), dalpha(0.0), useSwitchingFunction(false), useDispersionCorrection(true), exceptionsUsePeriodic(false), recipForceGroup(-1),
        includeDirectSpace(true), nx(0), ny(0), nz(0), dnx(0), dny(0), dnz(0) {
}

NativeNonbondedForce::NativeNonbondedForce(const NonbondedForce& force) {
    nonbondedMethod = static_cast<NonbondedMethod>(force.getNonbondedMethod());
    cutoffDistance = force.getCutoffDistance();
    switchingDistance = force.getSwitchingDistance();
    rfDielectric = force.getReactionFieldDielectric();
    ewaldErrorTol = force.getEwaldErrorTolerance();
    force.getPMEParameters(alpha, nx, ny, nz);
    force.getLJPMEParameters(dalpha, dnx, dny, dnz);
    useSwitchingFunction = force.getUseSwitchingFunction();
    useDispersionCorrection = force.getUseDispersionCorrection();
    exceptionsUsePeriodic = force.getExceptionsUsePeriodicBoundaryConditions();
    recipForceGroup = force.getReciprocalSpaceForceGroup();
    includeDirectSpace = force.getIncludeDirectSpace();

    for (int index = 0; index < force.getNumParticles(); index++) {
        double charge, sigma, epsilon;
        force.getParticleParameters(index, charge, sigma, epsilon);
        addParticle(charge, sigma, epsilon);
    }

    for (int index = 0; index < force.getNumExceptions(); index++) {
        int particle1, particle2;
        double chargeProd, sigma, epsilon;
        force.getExceptionParameters(index, particle1, particle2, chargeProd, sigma, epsilon);
        addException(particle1, particle2, chargeProd, sigma, epsilon);
    }

    for (int index = 0; index < force.getNumGlobalParameters(); index++)
        addGlobalParameter(force.getGlobalParameterName(index),
                           force.getGlobalParameterDefaultValue(index));

    for (int index = 0; index < force.getNumParticleParameterOffsets(); index++) {
        std::string parameter;
        int particleIndex;
        double chargeScale, sigmaScale, epsilonScale;
        force.getParticleParameterOffset(index, parameter, particleIndex, chargeScale, sigmaScale, epsilonScale);
        addParticleParameterOffset(parameter, particleIndex, chargeScale, sigmaScale, epsilonScale);
    }

    for (int index = 0; index < force.getNumExceptionParameterOffsets(); index++) {
        std::string parameter;
        int exceptionIndex;
        double chargeProdScale, sigmaScale, epsilonScale;
        force.getExceptionParameterOffset(index, parameter, exceptionIndex, chargeProdScale, sigmaScale, epsilonScale);
        addExceptionParameterOffset(parameter, exceptionIndex, chargeProdScale, sigmaScale, epsilonScale);
    }
}

NativeNonbondedForce::NonbondedMethod NativeNonbondedForce::getNonbondedMethod() const {
    return nonbondedMethod;
}

void NativeNonbondedForce::setNonbondedMethod(NonbondedMethod method) {
    if (method < 0 || method > 5)
        throw OpenMMException("NativeNonbondedForce: Illegal value for nonbonded method");
    nonbondedMethod = method;
}

double NativeNonbondedForce::getCutoffDistance() const {
    return cutoffDistance;
}

void NativeNonbondedForce::setCutoffDistance(double distance) {
    cutoffDistance = distance;
}

bool NativeNonbondedForce::getUseSwitchingFunction() const {
    return useSwitchingFunction;
}

void NativeNonbondedForce::setUseSwitchingFunction(bool use) {
    useSwitchingFunction = use;
}

double NativeNonbondedForce::getSwitchingDistance() const {
    return switchingDistance;
}

void NativeNonbondedForce::setSwitchingDistance(double distance) {
    switchingDistance = distance;
}

double NativeNonbondedForce::getReactionFieldDielectric() const {
    return rfDielectric;
}

void NativeNonbondedForce::setReactionFieldDielectric(double dielectric) {
    rfDielectric = dielectric;
}

double NativeNonbondedForce::getEwaldErrorTolerance() const {
    return ewaldErrorTol;
}

void NativeNonbondedForce::setEwaldErrorTolerance(double tol) {
    ewaldErrorTol = tol;
}

void NativeNonbondedForce::getPMEParameters(double& alpha, int& nx, int& ny, int& nz) const {
    alpha = this->alpha;
    nx = this->nx;
    ny = this->ny;
    nz = this->nz;
}

void NativeNonbondedForce::getLJPMEParameters(double& alpha, int& nx, int& ny, int& nz) const {
    alpha = this->dalpha;
    nx = this->dnx;
    ny = this->dny;
    nz = this->dnz;
}

void NativeNonbondedForce::setPMEParameters(double alpha, int nx, int ny, int nz) {
    this->alpha = alpha;
    this->nx = nx;
    this->ny = ny;
    this->nz = nz;
}

void NativeNonbondedForce::setLJPMEParameters(double alpha, int nx, int ny, int nz) {
    this->dalpha = alpha;
    this->dnx = nx;
    this->dny = ny;
    this->dnz = nz;
}

void NativeNonbondedForce::getPMEParametersInContext(const Context& context, double& alpha, int& nx, int& ny, int& nz) const {
    dynamic_cast<const NativeNonbondedForceImpl&>(getImplInContext(context)).getPMEParameters(alpha, nx, ny, nz);
}

void NativeNonbondedForce::getLJPMEParametersInContext(const Context& context, double& alpha, int& nx, int& ny, int& nz) const {
    dynamic_cast<const NativeNonbondedForceImpl&>(getImplInContext(context)).getLJPMEParameters(alpha, nx, ny, nz);
}

int NativeNonbondedForce::addParticle(double charge, double sigma, double epsilon) {
    particles.push_back(ParticleInfo(charge, sigma, epsilon));
    return particles.size()-1;
}

void NativeNonbondedForce::getParticleParameters(int index, double& charge, double& sigma, double& epsilon) const {
    ASSERT_VALID_INDEX(index, particles);
    charge = particles[index].charge;
    sigma = particles[index].sigma;
    epsilon = particles[index].epsilon;
}

void NativeNonbondedForce::setParticleParameters(int index, double charge, double sigma, double epsilon) {
    ASSERT_VALID_INDEX(index, particles);
    particles[index].charge = charge;
    particles[index].sigma = sigma;
    particles[index].epsilon = epsilon;
}

int NativeNonbondedForce::addException(int particle1, int particle2, double chargeProd, double sigma, double epsilon, bool replace) {
    map<pair<int, int>, int>::iterator iter = exceptionMap.find(pair<int, int>(particle1, particle2));
    int newIndex;
    if (iter == exceptionMap.end())
        iter = exceptionMap.find(pair<int, int>(particle2, particle1));
    if (iter != exceptionMap.end()) {
        if (!replace) {
            stringstream msg;
            msg << "NativeNonbondedForce: There is already an exception for particles ";
            msg << particle1;
            msg << " and ";
            msg << particle2;
            throw OpenMMException(msg.str());
        }
        exceptions[iter->second] = ExceptionInfo(particle1, particle2, chargeProd, sigma, epsilon);
        newIndex = iter->second;
        exceptionMap.erase(iter->first);
    }
    else {
        exceptions.push_back(ExceptionInfo(particle1, particle2, chargeProd, sigma, epsilon));
        newIndex = exceptions.size()-1;
    }
    exceptionMap[pair<int, int>(particle1, particle2)] = newIndex;
    return newIndex;
}
void NativeNonbondedForce::getExceptionParameters(int index, int& particle1, int& particle2, double& chargeProd, double& sigma, double& epsilon) const {
    ASSERT_VALID_INDEX(index, exceptions);
    particle1 = exceptions[index].particle1;
    particle2 = exceptions[index].particle2;
    chargeProd = exceptions[index].chargeProd;
    sigma = exceptions[index].sigma;
    epsilon = exceptions[index].epsilon;
}

void NativeNonbondedForce::setExceptionParameters(int index, int particle1, int particle2, double chargeProd, double sigma, double epsilon) {
    ASSERT_VALID_INDEX(index, exceptions);
    exceptions[index].particle1 = particle1;
    exceptions[index].particle2 = particle2;
    exceptions[index].chargeProd = chargeProd;
    exceptions[index].sigma = sigma;
    exceptions[index].epsilon = epsilon;
}

ForceImpl* NativeNonbondedForce::createImpl() const {
    return new NativeNonbondedForceImpl(*this);
}

void NativeNonbondedForce::createExceptionsFromBonds(const vector<pair<int, int> >& bonds, double coulomb14Scale, double lj14Scale) {
    for (auto& bond : bonds)
        if (bond.first < 0 || bond.second < 0 || bond.first >= particles.size() || bond.second >= particles.size())
            throw OpenMMException("createExceptionsFromBonds: Illegal particle index in list of bonds");

    // Find particles separated by 1, 2, or 3 bonds.

    vector<set<int> > exclusions(particles.size());
    vector<set<int> > bonded12(exclusions.size());
    for (auto& bond : bonds) {
        bonded12[bond.first].insert(bond.second);
        bonded12[bond.second].insert(bond.first);
    }
    for (int i = 0; i < (int) exclusions.size(); ++i)
        addExclusionsToSet(bonded12, exclusions[i], i, i, 2);

    // Find particles separated by 1 or 2 bonds and create the exceptions.

    for (int i = 0; i < (int) exclusions.size(); ++i) {
        set<int> bonded13;
        addExclusionsToSet(bonded12, bonded13, i, i, 1);
        for (int j : exclusions[i]) {
            if (j < i) {
                if (bonded13.find(j) == bonded13.end()) {
                    // This is a 1-4 interaction.

                    const ParticleInfo& particle1 = particles[j];
                    const ParticleInfo& particle2 = particles[i];
                    const double chargeProd = coulomb14Scale*particle1.charge*particle2.charge;
                    const double sigma = 0.5*(particle1.sigma+particle2.sigma);
                    const double epsilon = lj14Scale*std::sqrt(particle1.epsilon*particle2.epsilon);
                    addException(j, i, chargeProd, sigma, epsilon);
                }
                else {
                    // This interaction should be completely excluded.

                    addException(j, i, 0.0, 1.0, 0.0);
                }
            }
        }
    }
}

void NativeNonbondedForce::addExclusionsToSet(const vector<set<int> >& bonded12, set<int>& exclusions, int baseParticle, int fromParticle, int currentLevel) const {
    for (int i : bonded12[fromParticle]) {
        if (i != baseParticle)
            exclusions.insert(i);
        if (currentLevel > 0)
            addExclusionsToSet(bonded12, exclusions, baseParticle, i, currentLevel-1);
    }
}

int NativeNonbondedForce::addGlobalParameter(const string& name, double defaultValue) {
    globalParameters.push_back(GlobalParameterInfo(name, defaultValue));
    return globalParameters.size()-1;
}

const string& NativeNonbondedForce::getGlobalParameterName(int index) const {
    ASSERT_VALID_INDEX(index, globalParameters);
    return globalParameters[index].name;
}

void NativeNonbondedForce::setGlobalParameterName(int index, const string& name) {
    ASSERT_VALID_INDEX(index, globalParameters);
    globalParameters[index].name = name;
}

double NativeNonbondedForce::getGlobalParameterDefaultValue(int index) const {
    ASSERT_VALID_INDEX(index, globalParameters);
    return globalParameters[index].defaultValue;
}

void NativeNonbondedForce::setGlobalParameterDefaultValue(int index, double defaultValue) {
    ASSERT_VALID_INDEX(index, globalParameters);
    globalParameters[index].defaultValue = defaultValue;
}

int NativeNonbondedForce::getGlobalParameterIndex(const std::string& parameter) const {
    for (int i = 0; i < globalParameters.size(); i++)
        if (globalParameters[i].name == parameter)
            return i;
    throw OpenMMException("There is no global parameter called '"+parameter+"'");
}

int NativeNonbondedForce::addParticleParameterOffset(const std::string& parameter, int particleIndex, double chargeScale, double sigmaScale, double epsilonScale) {
    particleOffsets.push_back(ParticleOffsetInfo(getGlobalParameterIndex(parameter), particleIndex, chargeScale, sigmaScale, epsilonScale));
    return particleOffsets.size()-1;
}

void NativeNonbondedForce::getParticleParameterOffset(int index, std::string& parameter, int& particleIndex, double& chargeScale, double& sigmaScale, double& epsilonScale) const {
    ASSERT_VALID_INDEX(index, particleOffsets);
    parameter = globalParameters[particleOffsets[index].parameter].name;
    particleIndex = particleOffsets[index].particle;
    chargeScale = particleOffsets[index].chargeScale;
    sigmaScale = particleOffsets[index].sigmaScale;
    epsilonScale = particleOffsets[index].epsilonScale;
}

void NativeNonbondedForce::setParticleParameterOffset(int index, const std::string& parameter, int particleIndex, double chargeScale, double sigmaScale, double epsilonScale) {
    ASSERT_VALID_INDEX(index, particleOffsets);
    particleOffsets[index].parameter = getGlobalParameterIndex(parameter);
    particleOffsets[index].particle = particleIndex;
    particleOffsets[index].chargeScale = chargeScale;
    particleOffsets[index].sigmaScale = sigmaScale;
    particleOffsets[index].epsilonScale = epsilonScale;
}

int NativeNonbondedForce::addExceptionParameterOffset(const std::string& parameter, int exceptionIndex, double chargeProdScale, double sigmaScale, double epsilonScale) {
    exceptionOffsets.push_back(ExceptionOffsetInfo(getGlobalParameterIndex(parameter), exceptionIndex, chargeProdScale, sigmaScale, epsilonScale));
    return exceptionOffsets.size()-1;
}

void NativeNonbondedForce::getExceptionParameterOffset(int index, std::string& parameter, int& exceptionIndex, double& chargeProdScale, double& sigmaScale, double& epsilonScale) const {
    ASSERT_VALID_INDEX(index, exceptionOffsets);
    parameter = globalParameters[exceptionOffsets[index].parameter].name;
    exceptionIndex = exceptionOffsets[index].exception;
    chargeProdScale = exceptionOffsets[index].chargeProdScale;
    sigmaScale = exceptionOffsets[index].sigmaScale;
    epsilonScale = exceptionOffsets[index].epsilonScale;
}

void NativeNonbondedForce::setExceptionParameterOffset(int index, const std::string& parameter, int exceptionIndex, double chargeProdScale, double sigmaScale, double epsilonScale) {
    ASSERT_VALID_INDEX(index, exceptionOffsets);
    exceptionOffsets[index].parameter = getGlobalParameterIndex(parameter);
    exceptionOffsets[index].exception = exceptionIndex;
    exceptionOffsets[index].chargeProdScale = chargeProdScale;
    exceptionOffsets[index].sigmaScale = sigmaScale;
    exceptionOffsets[index].epsilonScale = epsilonScale;
}

int NativeNonbondedForce::getReciprocalSpaceForceGroup() const {
    return recipForceGroup;
}

void NativeNonbondedForce::setReciprocalSpaceForceGroup(int group) {
    if (group < -1 || group > 31)
        throw OpenMMException("Force group must be between -1 and 31");
    recipForceGroup = group;
}

bool NativeNonbondedForce::getIncludeDirectSpace() const {
    return includeDirectSpace;
}

void NativeNonbondedForce::setIncludeDirectSpace(bool include) {
    includeDirectSpace = include;
}

void NativeNonbondedForce::updateParametersInContext(Context& context) {
    dynamic_cast<NativeNonbondedForceImpl&>(getImplInContext(context)).updateParametersInContext(getContextImpl(context));
}

bool NativeNonbondedForce::getExceptionsUsePeriodicBoundaryConditions() const {
    return exceptionsUsePeriodic;
}

void NativeNonbondedForce::setExceptionsUsePeriodicBoundaryConditions(bool periodic) {
    exceptionsUsePeriodic = periodic;
}
