#include "Ec.h"
#include <math.h>
#include <netcdf.h>
#include <assert.h>
#include <stdlib.h>
#include "../Util.h"

FileEc::FileEc(std::string iFilename, const Options& iOptions, bool iReadOnly) : FileNetcdf(iFilename, iOptions, iReadOnly) {
   // Set dimensions
   int dTime = getDim("time");
   int dEns  = getDim("ensemble_member");
   int dLon  = getLonDim();
   int dLat  = getLatDim();
   mNTime = getDimSize("time");
   mNEns  = getDimSize("ensemble_member");
   mNLat  = getDimSize(dLat);
   mNLon  = getDimSize(dLon);

   // Retrieve lat/lon/elev
   int vLat = getLatVar();
   int vLon = getLonVar();
   mLats  = getGridValues(vLat);
   mLons  = getGridValues(vLon);

   // Elevations are set in this order:
   // - use altitude if it is in the file
   // - Use surface geopotential if it is available and if it has lat/lon dimensions
   //   and ignore any other dimensions
   // - Set elevations to missing
   if(hasVar("altitude")) {
      int vElev = getVar("altitude");
      mElevs = getGridValues(vElev);
   }
   else {
      bool hasValidGeopotential = false;
      if(hasVar("surface_geopotential")) {
         // Check if geopotential is a valid field and if so convert to altitude
         // Can't use getFieldCore here since the field might not have the time dimension. Therefore
         // manually parse the variable.
         int vGeopotential = getVar("surface_geopotential");
         int N = getNumDims(vGeopotential);
         size_t count[N];
         size_t start[N];
         int size = 1;
         int dims[N];
         int indexLat = Util::MV;
         int indexLon = Util::MV;
         nc_inq_vardimid(mFile, vGeopotential, dims);
         for(int i = 0; i < N; i++) {
            if(dims[i] == getLatDim()) {
               count[i] = getNumLat();
               size *= count[i];
               indexLat = i;
            }
            else if(dims[i] == getLonDim()) {
               count[i] = getNumLon();
               size *= count[i];
               indexLon = i;
            }
            else {
               size_t dimsize = 1;
               nc_inq_dimlen(mFile, dims[i], &dimsize);
               if(dimsize > 1){
                  std::stringstream ss;
                  ss << "surface_geopotential has an extra non-singleton dimension (dim " << i << ") of length " << dimsize << ". Using index 0 to extract lat/lon/elev.";
                  Util::warning(ss.str());
               }
               count[i] = 1;
            }
            start[i] = 0;
         }
         hasValidGeopotential = Util::isValid(indexLat) && Util::isValid(indexLon);
         if(hasValidGeopotential) {
            float* values = new float[size];
            nc_get_vara_float(mFile, vGeopotential, start, count, values);
            mElevs.resize(getNumLat());
            for(int i = 0; i < getNumLat(); i++) {
               mElevs[i].resize(getNumLon());
               for(int j = 0; j < getNumLon(); j++) {
                  // Latitude dimension is ordered first
                  float value = Util::MV;
                  if(indexLat < indexLon) {
                     value = values[i*getNumLon() + j];
                  }
                  // Longitude dimension is ordered first
                  else {
                     value = values[j*getNumLat() + i];
                  }
                  mElevs[i][j] = value / 9.81;
               }
            }
            std::stringstream ss;
            ss << "Deriving altitude from geopotential height in " << getFilename();
            Util::warning(ss.str());
         }
         else {
            Util::warning("Surface geopotential does not have lat/lon dimensions. Cannot compute altitude.");
         }
      }
      if(!hasValidGeopotential) {
         mElevs.resize(getNumLat());
         for(int i = 0; i < getNumLat(); i++) {
            mElevs[i].resize(getNumLon());
            for(int j = 0; j < getNumLon(); j++) {
               mElevs[i][j] = Util::MV;
            }
         }
         Util::warning("No altitude field available in " + getFilename());
      }
   }

   // TODO: No land fraction info in EC files?
   if(hasVar("land_area_fraction")) {
      int vLandFraction = getVar("land_area_fraction");
      mLandFractions = getGridValues(vLandFraction);
   }
   else {
      mLandFractions.resize(getNumLat());
      for(int i = 0; i < getNumLat(); i++) {
         mLandFractions[i].resize(getNumLon());
         for(int j = 0; j < getNumLon(); j++) {
            mLandFractions[i][j] = Util::MV;
         }
      }
   }

   if(hasVar("time")) {
      int vTime = getVar("time");
      double* times = new double[mNTime];
      int status = nc_get_var_double(mFile, vTime, times);
      handleNetcdfError(status, "could not get times");
      setTimes(std::vector<double>(times, times+mNTime));
      delete[] times;
   }
   else {
      std::vector<double> times;
      times.resize(getNumTime(), Util::MV);
      setTimes(times);
   }

   if(hasVar("forecast_reference_time")) {
      int vReferenceTime = getVar("forecast_reference_time");
      double referenceTime = getReferenceTime();
      int status = nc_get_var_double(mFile, vReferenceTime, &referenceTime);
      handleNetcdfError(status, "could not get reference time");
      setReferenceTime(referenceTime);
   }

   Util::status( "File '" + iFilename + " 'has dimensions " + getDimenionString());
}

FieldPtr FileEc::getFieldCore(Variable::Type iVariable, int iTime) const {
   std::string variableName = getVariableName(iVariable);
   return getFieldCore(variableName, iTime);
}

FieldPtr FileEc::getFieldCore(std::string iVariable, int iTime) const {
   startDataMode();
   // Not cached, retrieve data
   int var = getVar(iVariable);
   int nTime = mNTime;
   int nEns  = mNEns;
   int nLat  = mNLat;
   int nLon  = mNLon;

   size_t count[5] = {1, 1, nEns, nLat, nLon};
   size_t start[5] = {iTime, 0, 0, 0, 0};
   size_t size = 1*1*nEns*nLat*nLon;
   float* values = new float[size];
   nc_get_vara_float(mFile, var, start, count, values);
   float MV = getMissingValue(var);

   float offset = getOffset(var);
   float scale = getScale(var);
   int index = 0;
   FieldPtr field = getEmptyField();
   for(int e = 0; e < nEns; e++) {
      for(int lat = 0; lat < nLat; lat++) {
         for(int lon = 0; lon < nLon; lon++) {
            float value = values[index];
            if(Util::isValid(MV) && value == MV) {
               // Field has missing value indicator and the value is missing
               // Save values using our own internal missing value indicator
               value = Util::MV;
            }
            else {
               value = scale*values[index] + offset;
            }
            (*field)(lat,lon,e) = value;
            index++;
         }
      }
   }
   delete[] values;
   return field;
}

void FileEc::writeCore(std::vector<Variable::Type> iVariables) {
   startDefineMode();

   // check if altitudes are valid
   bool isAltitudeValid = false;
   vec2 elevs = getElevs();
   for(int i = 0; i < elevs.size(); i++) {
      for(int j = 0; j < elevs[i].size(); j++) {
         isAltitudeValid = isAltitudeValid || Util::isValid(elevs[i][j]);
      }
   }
   if(isAltitudeValid && !hasVar("altitude")) {
      defineAltitude();
   }

   // Define variables
   for(int v = 0; v < iVariables.size(); v++) {
      Variable::Type varType = iVariables[v];
      std::string variable = getVariableName(varType);
      std::string typeName = Variable::getTypeName(varType);

      if(variable == "") {
         Util::error("Cannot write variable '" + typeName + "' because there EC output file has no definition for it");
      }
      if(!hasVariableCore(varType)) {
         // Create variable
         int dTime    = getDim("time");
         int dSurface = getDim("surface");
         int dEns     = getDim("ensemble_member");
         int dLon = getLonDim();
         int dLat = getLatDim();
         int dims[5] = {dTime, dSurface, dEns, dLat, dLon};
         int var = Util::MV;
         int status = nc_def_var(mFile, variable.c_str(), NC_FLOAT, 5, dims, &var);
         handleNetcdfError(status, "could not define variable '" + variable + "'");
      }
      int var = getVar(variable);
      float MV = getMissingValue(var); // The output file's missing value indicator
      // TODO: Automatically determine if this should be "lon lat" or "longitude latitude"
      setAttribute(var, "coordinates", "lon lat");
      setAttribute(var, "units", Variable::getUnits(varType));
      setAttribute(var, "standard_name", Variable::getStandardName(varType));
   }
   defineTimes();
   defineReferenceTime();
   defineGlobalAttributes();
   startDataMode();

   writeTimes();
   writeReferenceTime();
   if(isAltitudeValid) {
      writeAltitude();
   }
   for(int v = 0; v < iVariables.size(); v++) {
      Variable::Type varType = iVariables[v];
      std::string variable = getVariableName(varType);
      assert(hasVariableCore(varType));
      int var = getVar(variable);
      float MV = getMissingValue(var); // The output file's missing value indicator
      size_t size = 1*1*mNEns*mNLat*mNLon;
      float* values = new float[size];
      for(int t = 0; t < mNTime; t++) {
         float offset = getOffset(var);
         float scale = getScale(var);
         FieldPtr field = getField(varType, t);
         if(field != NULL) { // TODO: Can't be null if coming from reference
            size_t start[5] = {t, 0, 0, 0, 0};

            int index = 0;
            for(int e = 0; e < mNEns; e++) {
               for(int lat = 0; lat < mNLat; lat++) {
                  for(int lon = 0; lon < mNLon; lon++) {
                     float value = (*field)(lat,lon,e);
                     if(Util::isValid(MV) && !Util::isValid(value)) {
                        // Field has missing value indicator and the value is missing
                        // Save values using the file's missing indicator value
                        value = MV;
                     }
                     else {
                        value = ((*field)(lat,lon,e) - offset)/scale;
                     }
                     values[index] = value;
                     index++;
                  }
               }
            }
            int numDims = getNumDims(var);
            if(numDims == 5) {
               size_t count[5] = {1,1,mNEns, mNLat, mNLon};
               int status = nc_put_vara_float(mFile, var, start, count, values);
               handleNetcdfError(status, "could not write variable " + variable);
            }
            else {
               std::stringstream ss;
               ss << "Cannot write " << variable << " to '" << getFilename() <<
                             "' because it does not have 5 dimensions. It has " << numDims << " dimensions.";
               Util::warning(ss.str());
            }
         }
      }
      delete[] values;
   }
}


std::string FileEc::getVariableName(Variable::Type iVariable) const {
   if(iVariable == Variable::PrecipAcc) {
      return "precipitation_amount_acc";
   }
   else if(iVariable == Variable::Cloud) {
      return "cloud_area_fraction";
   }
   else if(iVariable == Variable::T) {
      return "air_temperature_2m";
   }
   else if(iVariable == Variable::TMin) {
      return "air_temperature_2m_min6h";
   }
   else if(iVariable == Variable::TMax) {
      return "air_temperature_2m_max6h";
   }
   else if(iVariable == Variable::TD) {
      return "dew_point_temperature_2m";
   }
   else if(iVariable == Variable::TW) {
      return "wet_bulb_temperature_2m";
   }
   else if(iVariable == Variable::Tlevel0) {
      return "air_temperature_ml";
   }
   else if(iVariable == Variable::Tlevel1) {
      return "air_temperature_ml";
   }
   else if(iVariable == Variable::Precip) {
      return "precipitation_amount";
   }
   else if(iVariable == Variable::PrecipRate) {
      return "lwe_precipitation_rate";
   }
   else if(iVariable == Variable::U) {
      return "eastward_wind_10m";
   }
   else if(iVariable == Variable::Xwind) {
      return "x_wind_10m";
   }
   else if(iVariable == Variable::V) {
      return "northward_wind_10m";
   }
   else if(iVariable == Variable::Ywind) {
      return "y_wind_10m";
   }
   else if(iVariable == Variable::W) {
      return "windspeed_10m";
   }
   else if(iVariable == Variable::MSLP) {
      return "sea_level_pressure";
   }
   else if(iVariable == Variable::RH) {
      return "relative_humidity_2m";
   }
   else if(iVariable == Variable::Symbol) {
      return "weather_symbol";
   }
   return "";
}

bool FileEc::isValid(std::string iFilename) {
   bool isValid = false;
   int file;
   int status = nc_open(iFilename.c_str(), NC_NOWRITE, &file);
   if(status == NC_NOERR) {
      isValid = hasDim(file, "time") &&
               (hasVar(file, "lat") || hasVar(file, "latitude")) &&
               (hasVar(file, "lon") || hasVar(file, "longitude")) &&
               hasDim(file, "ensemble_member") &&
               (hasDim(file, "lat") || hasDim(file, "latitude")  || hasDim(file, "y")) &&
               (hasDim(file, "lon") || hasDim(file, "longitude") || hasDim(file, "x"));
      nc_close(file);
   }
   return isValid;
}
vec2 FileEc::getGridValues(int iVar) const {
   // Initialize values
   vec2 grid;
   grid.resize(getNumLat());
   for(int i = 0; i < getNumLat(); i++) {
      grid[i].resize(getNumLon(), Util::MV);
   }

   // We have a lat/lon grid, where lat/lons are only provided along the pertinent dimension
   // Values are assumed to be constant across the other dimension.
   int numDims = getNumDims(iVar);
   if(numDims == 1) {
      int dim;
      nc_inq_vardimid(mFile, iVar, &dim);
      long size = getDimSize(dim);
      float* values = new float[size];
      nc_get_var_float(mFile, iVar, values);
      // Latitude variable
      if(dim == getLatDim()) {
         for(int i = 0; i < getNumLat(); i++) {
            for(int j = 0; j < getNumLon(); j++) {
               grid[i][j] = values[i];
            }
         }
      }
      // Longitude variable
      else if(dim == getLonDim()) {
         for(int i = 0; i < getNumLat(); i++) {
            for(int j = 0; j < getNumLon(); j++) {
               grid[i][j] = values[j];
            }
         }
      }
      else {
         std::stringstream ss;
         ss << "Missing lat or lon dimension";
         Util::error(ss.str());
      }
      delete[] values;
   }
   // We have a projected grid, where lat and lons are provided for each grid point
   else {
      int N = getNumDims(iVar);
      size_t count[N];
      size_t start[N];
      int size = 1;
      int indexLat = Util::MV;
      int indexLon = Util::MV;
      int dims[N];
      nc_inq_vardimid(mFile, iVar, dims);
      for(int i = 0; i < N; i++) {
         if(dims[i] == getLatDim()) {
            count[i] = getNumLat();
            size *= count[i];
            indexLat = i;
         }
         else if(dims[i] == getLonDim()) {
            count[i] = getNumLon();
            size *= count[i];
            indexLon = i;
         }
         else {
            size_t dimsize = 1;
            nc_inq_dimlen(mFile, dims[i], &dimsize);
            if(dimsize > 1){
               std::stringstream ss;
               ss << "Lat/lon/elev has an extra non-singleton dimension (dim " << i << ") of length " << dimsize << ". Using index 0 to extract lat/lon/elev.";
               Util::warning(ss.str());
            }
            count[i] = 1;
         }
         start[i] = 0;
      }
      if(!Util::isValid(indexLat) || !Util::isValid(indexLon)) {
         std::stringstream ss;
         ss << "Missing lat and/or lon dimensions";
         Util::error(ss.str());
      }
      float* values = new float[size];
      nc_get_vara_float(mFile, iVar, start, count, values);
      for(int i = 0; i < getNumLat(); i++) {
         for(int j = 0; j < getNumLon(); j++) {
            // Latitude dimension is ordered first
            if(indexLat < indexLon) {
               grid[i][j] = values[i*getNumLon() + j];
            }
            // Longitude dimension is ordered first
            else {
               grid[i][j] = values[j*getNumLat() + i];
            }
         }
      }
      delete[] values;
   }
   return grid;
}

int FileEc::getLatDim() const {
   int dLat;
   if(hasDim("y"))
      dLat = getDim("y");
   else if(hasDim("latitude"))
      dLat = getDim("latitude");
   else
      dLat = getDim("lat");
   return dLat;
}
int FileEc::getLonDim() const {
   int dLon;
   if(hasDim("x"))
      dLon = getDim("x");
   else if(hasDim("longitude"))
      dLon = getDim("longitude");
   else
      dLon = getDim("lon");
   return dLon;
}
int FileEc::getLatVar() const {
   int vLat;
   if(hasVar("latitude"))
      vLat = getVar("latitude");
   else
      vLat = getVar("lat");
   return vLat;
}
int FileEc::getLonVar() const {
   int vLon;
   if(hasVar("longitude"))
      vLon = getVar("longitude");
   else
      vLon = getVar("lon");
   return vLon;
}
std::string FileEc::description() {
   std::stringstream ss;
   ss << Util::formatDescription("type=ec", "ECMWF ensemble file") << std::endl;
   return ss.str();
}

void FileEc::defineAltitude() {
   // Determine the order of lat and lon dimensions in altitude field
   int indexLat = Util::MV;
   int indexLon = Util::MV;

   // First check if latitude variable has two dimensions
   int vUseDims = Util::MV;
   if(getNumDims(getLatVar()) >= 2) {
      vUseDims = getLatVar();
   }
   else if(hasVar("surface_geopotential")) {
      vUseDims = getVar("surface_geopotential");
   }
   if(Util::isValid(vUseDims)) {
      int N = getNumDims(vUseDims);
      int dimsLat[N];
      nc_inq_vardimid(mFile, vUseDims, dimsLat);
      for(int i = 0; i < N; i++) {
         if(dimsLat[i] == getLatDim())
            indexLat = i;
         if(dimsLat[i] == getLonDim())
            indexLon = i;
      }
   }
   else {
      Util::warning("Could not determine lat/lon ordering when creating altitude variable. Using [lat, lon]");
      indexLat = 0;
      indexLon = 1;
   }

   int dims[2];
   int dLat = getLatDim();
   int dLon = getLonDim();
   if(indexLat < indexLon) {
      dims[0]  = dLat;
      dims[1]  = dLon;
   }
   else {
      dims[0]  = dLon;
      dims[1]  = dLat;
   }
   int var = Util::MV;
   int status = nc_def_var(mFile, "altitude", NC_FLOAT, 2, dims, &var);
   handleNetcdfError(status, "could not define altitude");
}

void FileEc::writeAltitude() const {
   int vElev = getVar("altitude");
   int numDims = getNumDims(vElev);
   if(numDims == 1) {
      Util::error("Cannot write altitude when the variable only has one dimension");
   }
   vec2 elevs = getElevs();

   int N = getNumDims(vElev);
   size_t count[N];
   size_t start[N];
   int size = 1;
   int indexLat = Util::MV;
   int indexLon = Util::MV;
   int dims[N];
   nc_inq_vardimid(mFile, vElev, dims);
   for(int i = 0; i < N; i++) {
      if(dims[i] == getLatDim()) {
         count[i] = getNumLat();
         size *= count[i];
         indexLat = i;
      }
      else if(dims[i] == getLonDim()) {
         count[i] = getNumLon();
         size *= count[i];
         indexLon = i;
      }
      else {
         size_t dimsize = 1;
         nc_inq_dimlen(mFile, dims[i], &dimsize);
         if(dimsize > 1){
            std::stringstream ss;
            ss << "Lat/lon/elev has an extra non-singleton dimension (dim " << i << ") of length " << dimsize << ". Using index 0 to write lat/lon/elev.";
            Util::warning(ss.str());
         }
         count[i] = 1;
      }
      start[i] = 0;
   }
   float MV = getMissingValue(vElev);
   float* values = new float[size];
   for(int i = 0; i < getNumLat(); i++) {
      for(int j = 0; j < getNumLon(); j++) {
         float elev = elevs[i][j];
         if(Util::isValid(MV) && !Util::isValid(elev))
            elev = MV;
         // Latitude dimension is ordered first
         if(indexLat < indexLon) {
            values[i*getNumLon() + j] = elev;
         }
         // Longitude dimension is ordered first
         else {
            values[j*getNumLat() + i] = elev;
         }
      }
   }
   bool status = nc_put_vara_float(mFile, vElev, start, count, values);
   handleNetcdfError(status, "could not write altitude");
   delete[] values;
}
