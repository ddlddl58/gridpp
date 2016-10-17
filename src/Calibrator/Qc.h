#ifndef CALIBRATOR_QC_H
#define CALIBRATOR_QC_H
#include "Calibrator.h"
#include "../Variable.h"
#include "../Util.h"

class ParameterFile;
class Parameters;

//! Applies a quality control adjustment.
//! Ensures that all values are within an appropriate range.
class CalibratorQc : public Calibrator {
   public:
      CalibratorQc(Variable::Type iVariable, const Options& iOptions);
      static std::string description();
      std::string name() const override {return "qc";};
      bool requiresParameterFile() const override { return false;};
   private:
      bool calibrateCore(File& iFile, const ParameterFile* iParameterFile) const override;
      Variable::Type mVariable;
      float mMin;
      float mMax;
};
#endif
