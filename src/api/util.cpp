#include "gridpp.h"
#include <iostream>
#include <sys/time.h>
#include <stdlib.h>
#include <cmath>
#include <math.h>
#include <assert.h>

bool gridpp::util::is_valid(float value) {
    float MV = -999;
    return !std::isnan(value) && !std::isinf(value) && value != MV;
}
float gridpp::util::calculate_stat(const std::vector<float>& iArray, std::string iOperator, float iQuantile) {
   // Initialize to missing
   float MV = -999;
   float value = MV;
   if(iOperator == "mean" || iOperator == "sum") {
      float total = 0;
      int count = 0;
      for(int n = 0; n < iArray.size(); n++) {
         if(gridpp::util::is_valid(iArray[n])) {
            total += iArray[n];
            count++;
         }
      }
      if(count > 0) {
         if(iOperator == "mean")
            value = total / count;
         else
            value = total;
      }
   }
   else if(iOperator == "std") {
      // STD = sqrt(E[X^2] - E[X]^2)
      // The above formula is unstable when the variance is small and the mean is large.
      // Use the property that VAR(X) = VAR(X-K). Provided K is any element in the array,
      // the resulting calculation of VAR(X-K) is stable. Set K to the first non-missing value.
      float total  = 0;
      float total2 = 0;
      float K = MV;
      int count = 0;
      for(int n = 0; n < iArray.size(); n++) {
         if(gridpp::util::is_valid(iArray[n])) {
            if(!gridpp::util::is_valid(K))
               K = iArray[n];
            assert(gridpp::util::is_valid(K));

            total  += iArray[n] - K;
            total2 += (iArray[n] - K)*(iArray[n] - K);
            count++;
         }
      }
      if(count > 0) {
         float mean  = total / count;
         float mean2 = total2 / count;
         float var   = mean2 - mean*mean;
         if(var < 0) {
            // This should never happen
            var = 0;
            // Util::warning("CalibratorNeighbourhood: Problems computing std, unstable result. Setting value to 0");
         }
         float std = sqrt(var);
         value = std;
      }
   }
   else {
      if(iOperator == "min")
         iQuantile = 0;
      if(iOperator == "median")
         iQuantile = 0.5;
      if(iOperator == "max")
         iQuantile = 1;
      // Remove missing
      std::vector<float> cleanHood;
      cleanHood.reserve(iArray.size());
      for(int i = 0; i < iArray.size(); i++) {
         if(gridpp::util::is_valid(iArray[i]))
            cleanHood.push_back(iArray[i]);
      }
      int N = cleanHood.size();
      if(N > 0) {
         std::sort(cleanHood.begin(), cleanHood.end());
         int lowerIndex = floor(iQuantile * (N-1));
         int upperIndex = ceil(iQuantile * (N-1));
         float lowerQuantile = (float) lowerIndex / (N-1);
         float upperQuantile = (float) upperIndex / (N-1);
         float lowerValue = cleanHood[lowerIndex];
         float upperValue = cleanHood[upperIndex];
         if(lowerIndex == upperIndex) {
            value = lowerValue;
         }
         else {
            assert(upperQuantile > lowerQuantile);
            assert(iQuantile >= lowerQuantile);
            float f = (iQuantile - lowerQuantile)/(upperQuantile - lowerQuantile);
            assert(f >= 0);
            assert(f <= 1);
            value   = lowerValue + (upperValue - lowerValue) * f;
         }
      }
   }
   return value;
}
int gridpp::util::num_missing_values(const vec2& iArray) {
   int count = 0;
   for(int y = 0; y < iArray.size(); y++) {
      for(int x = 0; x < iArray[y].size(); x++) {
         count += !gridpp::util::is_valid(iArray[y][x]);
      }
   }
   return count;
}
