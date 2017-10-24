#ifndef VARIABLE_H
#define VARIABLE_H
#include <iostream>
#include <vector>

//! Represents a meteorological variable and its metadata
class Variable {
   public:
      //! Variable types
      enum Type {
         Precip       = 0,    // Hourly amount ending at the specified time
         PrecipAcc    = 1,    // Accumulated ending at the specified time
         Pop          = 2,    // Probability of precipitation
         Pop6h        = 3,    // Probability of precipitation for the last 6 hours
         PrecipLow    = 4,    // Low estimate of precipitation
         PrecipMiddle = 5,    // Middle estimate of precipitation
         PrecipHigh   = 6,    // High estimate of precipitation
         PrecipRate   = 7,    // Precipitation rate
         Cloud        = 10,   // Cloud cover (between 0 and 1)
         T            = 20,   // 2m temperature (K)
         TMin         = 21,   // 2m temperature minimum temperature (K)
         TMax         = 22,   // 2m temperature maximum temperature (K)
         TD           = 23,   // 2m dew point temperature (K)
         Tlevel0      = 24,   // temperature at the lowest model level (K)
         Tlevel1      = 25,   // temperature at the second lowest model level (K)
         U            = 30,   // 10m U-wind (m/s)
         Xwind        = 31,   // 10m X-wind (m/s)
         V            = 40,   // 10m V-wind (m/s)
         Ywind        = 41,   // 10m Y-wind (m/s)
         W            = 50,   // 10m windspeed (m/s)
         WD           = 55,   // Wind direction (degrees, from north 0)
         RH           = 60,   // Reliative humidity (%)
         Phase        = 70,   // Precip phase
         P            = 80,   // Surface pressure (pa)
         MSLP         = 85,   // Mean sea-level pressure (pa)
         QNH          = 88,   // Pressure reduced to sea-level using standard atmosphere (ICAO) (pa)
         SwinAcc      = 100,  // Accumulated incoming shortware radiation
         LwinAcc      = 101,  // Accumulated incoming longwave radiation
         Symbol       = 200,  // Weather symbol code
         Fake         = 1000, // Fake variable used for testing
         None         = -999  // Non-existant variable
      };

      //! Convert type to string
      static std::string getTypeName(Type iType);

      //! Get the minimum possible attainable value for this variable
      static float getMin(Type iType);

      //! Get the maximum possible attainable value for this variable
      static float getMax(Type iType);

      //! Convert string to type
      static Type getType(std::string iName);

      //! Returns the units of the variable
      static std::string getUnits(Type iType);

      //! Returns the NetcdfCF standard name
      static std::string getStandardName(Type iType);

      //! Description of all defined variables
      static std::string getDescriptions();

      //! Vector of all defined variables
      static std::vector<Type> getAllVariables();

      //! Precipitation phase
      enum Phase {
         PhaseNone  = 0,
         PhaseRain  = 1,
         PhaseSleet = 2,
         PhaseSnow  = 3
      };
};
#endif
