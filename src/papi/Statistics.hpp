#pragma once

#include <vector>
#include <iostream>
#include <iomanip>

#include <limits>

namespace papi {
    /// @brief Represents statistics of a vector of values
    /// @tparam T numeric type
    template<typename T>
    struct Statistics {
        const T min;
        const T max;
        const T sum;
        const T mean;
        // positions
        const size_t size;
        const int indexMin;
        const int indexMax;
        // statistics
        const double average;
        const double median;

        static Statistics calculate(const std::vector<T>& data);
        static std::vector<T> diff(const std::vector<T>& data);
        void print(std::ostream& os = std::cout);

    private:
        Statistics();
        Statistics(T max, T min, T sum, T mean, size_t size, 
            int indexMin, int indexMax, 
            double average, double median);
    };

    template<typename T>
    std::vector<T> Statistics<T>::diff(const std::vector<T>& data) {
        std::vector<T> diff;
        T prev{0};
        for(const auto& value: data) {
            diff.push_back(value - prev);
            prev = value;
        }
        return diff;
    }

    template<typename T>
    Statistics<T> Statistics<T>::calculate(const std::vector<T>& data) {
        auto size = data.size();
        if ( size == 0 ) return std::move(Statistics<T>()); // noting to be calculated

        T min{std::numeric_limits<T>::max()};
        T max{std::numeric_limits<T>::lowest()};
        T sum{0};
        T mean{0};
        int indexMin{-1};
        int indexMax{-1};
        double average{0.0};
        double median{0.0};

        int index = 0;
        for(const T& value : data) {
            sum += value;
            if ( value < min ) {
                min = value;
                indexMin = index;
            }
            if ( value > max ) {
                max = value;
                indexMax = index;
            }
            ++index;
        }

        if ( size > 1 ) {
            mean = sum / size;
            average = (double)sum / (double)size;

            // median
            std::vector<T> copy{data};
            auto n = size / 2;
            std::nth_element(copy.begin(), copy.begin() + n, copy.end());
            median = copy[n];
            if ( size % 2 == 0 ) {
                std::nth_element(copy.begin(), copy.begin() + n - 1, copy.end());
                median = 0.5 * ( median + copy[n-1] );
            }
        } else {
            // single element sample
            mean = data[0];
            average = median = (double)mean;
        }
        return std::move(Statistics<T>{min, max, sum, mean, size, indexMin, indexMax, average, median});
    }

    template<typename T>
    void Statistics<T>::print(std::ostream& os) {
        os 
            << "sample size: " << size
            << ", min: " << min << " at " << indexMin
            << ", max: " << max << " at " << indexMax
            << ", mean: " << mean
            << std::setprecision(14)
            << ", average: " << average
            << ", median: " << median
            << std::endl;
    }

    template<typename T>
    Statistics<T>::Statistics()
        : min{0}
        , max{0}
        , sum{0}
        , mean{0}
        , size{0}
        , indexMin{-1}
        , indexMax{-1}
        , average{0.0}
        , median{0.0} {}

    template<typename T>
    Statistics<T>::Statistics(T min, T max, T sum, T mean, size_t size, 
            int indexMin, int indexMax, 
            double average, double median)
        : min{min}
        , max{max}
        , sum{sum}
        , mean{mean}
        , size{size}
        , indexMin{indexMin}
        , indexMax{indexMax}
        , average{average}
        , median{median} {}

}