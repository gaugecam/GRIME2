/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   Copyright 2021 Kenneth W. Chapman

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/** \file findline.h
 * @brief A file for a class to search an image for a horizontal water line
 *
 *
 * This file holds the class that searches for a horizontal in a line that features
 * a calibration target in water.
 *
 * The class also has methods to search for the top two bowties in a calibration
 * target to determine whether the target has moved relative to the camera since
 * the calibrarion was performed.
 *
 * The calibraton target is of the following type:
 *
 * \image html images/NRmarshDN-12-02-28-11-15_reference_image.jpg "Calibration Target"
 * \image latex images/NRmarshDN-12-02-28-11-15_reference_image.eps "Calibration Target"
 *
 * \author Kenneth W. Chapman
 * \copyright Copyright (C) 2010-2020, Kenneth W. Chapman <coffeesig@gmail.com>, all rights reserved.\n
 * This project is released under the 3-clause BSD License.
 * \bug No known bugs.
 */

#ifndef FINDLINE_H
#define FINDLINE_H

#include "gc_types.h"
#include "findcalibgrid.h"
#include <vector>
#include <random>
#include <opencv2/core.hpp>

namespace gc
{

/**
 * @brief Finds water level and detects calibration target movement (using a FindCalibGrid object)
 */
class FindLine
{
public:
    /**
     * @brief Constructor
     */
    FindLine();

    /**
     * @brief Destructor
     */
    ~FindLine() {}

    // TODO: Add doxygen comments -- KWC
    GC_STATUS SetLineFindAngleBounds( const double minAngle, const double maxAngle );

    /**
     * @brief Initializes the bowtie target templates in this objects instance of the calibration object
     * @param bowTieTemplateDim The template dimension will create an nxn template
     * @param imageSize Size of the image to be searched (for probablity space creation)
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS InitBowtieSearch( const int bowTieTemplateDim, const cv::Size imageSize );

    /**
     * @brief Given an image with a calibration target find the water level in the image
     * @param img The image to be searched
     * @param lines a vector of vertical lines that pass over the water line along which to be searched
     * @param result The result of the line search
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS Find( const cv::Mat &img, const std::vector< LineEnds > &lines, FindLineResult &result );

    // TODO: Add doxygen comments -- KWC
    GC_STATUS FitLineRANSAC( const std::vector< cv::Point2d > &pts, FindPointSet &findPtSet, const double xCenter, const cv::Mat &img );

    /**
     * @brief Method to search for the move targets using an instance of the FindCalibGrid class
     * @param img The image for which to search for the bowtie targets
     * @param ptsFound The found position of the move targets
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS FindMoveTargets( const cv::Mat &img, FindPointSet &ptsFound );

    /**
     * @brief Method to draw the found water line on an image as an overlay
     * @param img The image within which the water line was measured
     * @param imgOut New image of the input image with an overlay of the found water line
     * @param result The data for the found position of the water line to be drawn
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS DrawResult( const cv::Mat &img, cv::Mat &imgOut, const FindLineResult &result );

    /**
     * @brief Method to set the move target search regions
     * @param img Image within which to search for the targets to assure regions are valid
     * @param rect cv::Rect that holds the search region
     * @param isLeft true=Set the left region, false=Set the right region
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS SetMoveTargetROI( const cv::Mat &img, const cv::Rect rect, const bool isLeft );

private:
    FindCalibGrid m_findGrid;
    double m_minLineFindAngle;
    double m_maxLineFindAngle;
    std::default_random_engine m_randomEngine;

    GC_STATUS GetRandomNumbers( const int low_bound, const int high_bound, const int cnt_to_generate,
                                vector< int > &numbers, const bool isFirst );
    GC_STATUS CalcRowSums( const cv::Mat &img, const std::vector< LineEnds > &lines, std::vector< uint > &rowSums );
    GC_STATUS EvaluateSwath( const cv::Mat &img, const std::vector<LineEnds> &lines,
                             const size_t startIndex, const size_t endIndex, cv::Point2d &resultPt );
    GC_STATUS CalcSwathPoint( const std::vector< LineEnds > &swath, const std::vector< uint > &rowSums, cv::Point2d &resultPt );
    GC_STATUS MedianFilter( const size_t kernSize, const std::vector< uint > values, std::vector< uint > &valuesOut );

    GC_STATUS GetSlopeIntercept( const cv::Point2d one, const cv::Point2d two, double &slope, double &intercept );
    GC_STATUS DrawRowSums( const cv::Mat &img, std::vector< uint > rowSums, const std::vector< LineEnds > lines, cv::Mat &imgOut );
};

} // namespace gc

#endif // FINDLINE_H