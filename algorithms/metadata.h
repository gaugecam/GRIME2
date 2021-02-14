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

/** \file metadata.h
 * @brief A file for a class to add/retrieve metadata to/from images
 *
 * \author Kenneth W. Chapman
 * \copyright Copyright (C) 2010-2020, Kenneth W. Chapman <coffeesig@gmail.com>, all rights reserved.\n
 * This project is released under the 3-clause BSD License.
 * \bug No known bugs.
 */

#ifndef METADATA_H
#define METADATA_H

#include "gc_types.h"
#include <boost/property_tree/ptree.hpp>
#include "featuredata.h"
#include <libexif/exif-data.h>

namespace gc
{

// TODO: Write doxygen comments
//class ExifFeatures
//{
//public:
//    ExifFeatures() :
//        imageDims( -1, -1 ),
//        captureTime( "" ),
//        exposureTime( -1.0 ),
//        fNumber( -1.0 ),
//        isoSpeedRating( -1 ),
//        shutterSpeed( -1.0 )
//    {}

//    void clear()
//    {
//        imageDims = cv::Size( -1, -1 );
//        captureTime.clear();
//        exposureTime = -1.0;
//        fNumber = -1.0;
//        isoSpeedRating = -1;
//        shutterSpeed = -1.0;
//    }
//    cv::Size imageDims;
//    std::string captureTime;
//    double exposureTime;
//    double fNumber;
//    int isoSpeedRating;
//    double shutterSpeed;
//};

class MetaData
{
public:
    /**
     * @brief Constructor
     */
    MetaData() {}

    /**
     * @brief Destructor
     */
    ~MetaData() {}

    /**
     * @brief Reads metadata from an image to a FindData object
     * @param imgFilepath Filepath of the image from which to extract the metadata
     * @param data Object to which to return the metadata
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS ReadLineFindResult( const std::string imgFilepath, FindData &data );

    /**
     * @brief WriteLineFindResult Writes FindData values to an image as metadata in json format
     * @param imgFilepath Filepath of the image to which the data is written
     * @param data Object that holds the data to be written
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS WriteLineFindResult( const std::string imgFilepath, const FindData data );

    /**
     * @brief Retrieves the data to an std::string in the json format in which it was written
     * @param imgFilepath Filepath of the image from which to retrieve the metadata json string
     * @param jsonString std::string to which the metadata is retrieved
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS GetMetadata( const std::string imgFilepath, std::string &jsonString );

    // TODO Write doxygen comments
    GC_STATUS Retrieve( const std::string filepath, std::string &data, ExifFeatures &exifFeat );
private:
    static const std::string Version() { return "0.0.0.1"; }
    GC_STATUS ParseFindData( const std::string &metadata, FindData &data );
    GC_STATUS ParseFindPointSetString( const boost::property_tree::ptree &child, const std::string key, FindPointSet &ptSet );
    GC_STATUS FindResultToJsonString( const FindData data, std::string &jsonString );
    GC_STATUS CreateFindPointSetString( const FindPointSet set, const std::string key, std::string &jsonString );
    std::string ConvertToLocalTimestamp( const std::string exifTimestamp );
    GC_STATUS GetImageDescriptionExifData( const std::string imgFilepath, std::string &data );
    GC_STATUS GetExifTagString( const ExifData *exifData, const ExifTag tag, std::string &dataString );
};

} // namespace gc

#endif // METADATA_H