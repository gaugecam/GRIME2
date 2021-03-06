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

#include "log.h"
#include "findcalibgrid.h"
#include <cstdio>
#include <cmath>
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include <exception>

#ifdef DEBUG_FIND_CALIB_GRID
#undef DEBUG_FIND_CALIB_GRID
static const std::string DEBUG_RESULT_FOLDER = "/var/tmp/gaugecam/";
#endif

using namespace cv;
using namespace std;

namespace gc
{

FindCalibGrid::FindCalibGrid() :
    m_rectLeftMoveSearch( Rect( 0, 0, 5, 5 ) ),
    m_rectRightMoveSearch( Rect( 10, 0, 5, 5 ) )
{
}
GC_STATUS FindCalibGrid::InitBowtieTemplate( const int templateDim, const Size searchImgSize )
{
    GC_STATUS retVal = GC_OK;
    if ( 20 > templateDim || 1000 < templateDim )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::InitBowtieTemplate]"
                                                   " Invalid template dimension " << templateDim;
        retVal = GC_ERR;
    }
    else
    {
        int templateDimEven = templateDim + ( templateDim % 2 );

        int center = TEMPLATE_COUNT / 2;
        try
        {
            // create templates
            m_templates.clear();

            int tempDim = templateDimEven << 1;
            Mat matTemp( Size( tempDim, tempDim ), CV_8U );
            Mat matTempRot( Size( tempDim, tempDim ), CV_8U );

            for ( int i = 0; i < TEMPLATE_COUNT; ++i )
                m_templates.push_back( Mat( Size( templateDimEven, templateDimEven ), CV_8U ) );

            // create the unrotated center template
            matTemp.setTo( 224 );

            Point drawPoints[ 3 ];
            drawPoints[ 0 ] = Point( 1, 1 );
            drawPoints[ 1 ] = Point( 1, matTemp.rows - 2 );
            drawPoints[ 2 ] = Point( matTemp.cols / 2,
                               matTemp.rows / 2 );
            fillConvexPoly( matTemp, drawPoints, 3, Scalar( 32 ) );

            drawPoints[ 0 ] = Point( matTemp.cols - 2, 1 );
            drawPoints[ 1 ] = Point( matTemp.cols - 2,
                               matTemp.rows - 2 );
            drawPoints[ 2 ] = Point( matTemp.cols / 2,
                               matTemp.rows / 2 );
            fillConvexPoly( matTemp, drawPoints, 3, Scalar( 32 ) );

#ifdef DEBUG_FIND_CALIB_GRID   // debug of template rotation
            imwrite( DEBUG_RESULT_FOLDER + "_template_center.png", matTemp );
#endif

            // create the rotated templates
            Rect roiRotate( templateDimEven >> 1, templateDimEven >> 1, templateDimEven, templateDimEven );
            Mat matTemplateTemp = matTemp( roiRotate );
            matTemplateTemp.copyTo( m_templates[ static_cast< size_t >( center ) ] );

            for ( int i = 0; i < center; ++i )
            {
                retVal = RotateImage( matTemp, matTempRot, static_cast< double >( i - center ) );
                if ( GC_OK != retVal )
                    break;

                matTemplateTemp = matTempRot( roiRotate );
                matTemplateTemp.copyTo( m_templates[ static_cast< size_t >( i ) ] );

                retVal = RotateImage( matTemp, matTempRot, static_cast< double >( i + 1 ) );
                if ( 0 != retVal )
                    break;

                matTemplateTemp = matTempRot( roiRotate );
                matTemplateTemp.copyTo( m_templates[ static_cast< size_t >( center + i + 1 ) ] );
            }

            // allocate template match space
            m_matchSpace.create( Size( searchImgSize.width - templateDimEven + 1,
                                       searchImgSize.height - templateDimEven + 1 ), CV_32F );
            m_matchSpaceSmall.create( Size( ( templateDimEven >> 1 ) + 1, ( templateDimEven >> 1 ) + 1 ), CV_32F );

#ifdef DEBUG_FIND_CALIB_GRID   // debug of template rotation
            for ( size_t i = 0; i < TEMPLATE_COUNT; ++i )
            {
                imwrite( DEBUG_RESULT_FOLDER + "template_" + to_string( i ) + ".png", m_templates[ i ] );
            }
#endif
        }
        catch( exception &e )
        {
            FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::InitBowtieTemplate] " << e.what();
            retVal = GC_EXCEPT;
        }
    }
    return retVal;
}
GC_STATUS FindCalibGrid::RotateImage( const Mat &src, Mat &dst, const double angle )
{
    GC_STATUS retVal = GC_OK;
    try
    {
        Point2d ptCenter = Point2d( static_cast< double >( src.cols ) / 2.0, static_cast< double >( src.rows ) / 2.0 );
        Mat matRotMatrix = getRotationMatrix2D( ptCenter, angle, 1.0 );
        warpAffine( src, dst, matRotMatrix, dst.size(), INTER_CUBIC );
    }
    catch( exception &e )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::RotateImage] " << e.what();
        retVal = GC_EXCEPT;
    }
    return retVal;
}
GC_STATUS FindCalibGrid::FindTargets( const Mat &img, const double minScore, const string resultFilepath )
{
    GC_STATUS retVal = GC_OK;
    if ( m_templates.empty() )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::FindTargets] Templates not devined";
        retVal = GC_ERR;
    }
    else if ( img.empty() )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::FindTargets] Cannot find targets in a NULL image";
        retVal = GC_ERR;
    }
    if ( 0.01 > minScore || 1.0 < minScore )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::FindTargets] Invalid minimum target score " << minScore;
        retVal = GC_ERR;
    }
    else
    {
        retVal = MatchTemplate( TEMPLATE_COUNT >> 1, img, minScore, TARGET_COUNT * 2 );
        if ( GC_OK == retVal )
        {
            vector< TemplateBowtieItem > itemsTemp;
            for ( size_t i = 0; i < m_matchItems.size(); ++i )
                itemsTemp.push_back( m_matchItems[ i ] );

            m_matchItems.clear();
            for ( size_t i = 0; i < itemsTemp.size(); ++i )
            {
                for ( size_t j = 0; j < TEMPLATE_COUNT; ++j )
                {
                    retVal = MatchRefine( static_cast< int >( j ), img, minScore, 1, itemsTemp[ i ] );
                    if ( GC_OK != retVal )
                        break;
                }
                if ( GC_OK != retVal )
                    break;
                m_matchItems.push_back( itemsTemp[ i ] );
            }

            retVal = SortPoints( img.size() );

            if ( !resultFilepath.empty() )
            {
                Mat matTemp1;
                cvtColor( img, matTemp1, COLOR_GRAY2BGR );
                for ( size_t i = 0; i < m_matchItems.size(); ++i )
                {
                    line( matTemp1, Point( static_cast< int >( m_matchItems[ i ].pt.x ) - 5,
                                           static_cast< int >( m_matchItems[ i ].pt.y ) ),
                            Point( static_cast< int >( m_matchItems[ i ].pt.x ) + 5,
                                   static_cast< int >( m_matchItems[ i ].pt.y ) ), Scalar( 0, 0, 255 ) );
                    line( matTemp1, Point( static_cast< int >( m_matchItems[ i ].pt.x ),
                                           static_cast< int >( m_matchItems[ i ].pt.y ) - 5 ),
                            Point( static_cast< int >( m_matchItems[ i ].pt.x ),
                                   static_cast< int >( m_matchItems[ i ].pt.y ) + 5 ), Scalar( 0, 0, 255 ) );
                }
                bool isOK = imwrite( resultFilepath, matTemp1 );
                if ( !isOK )
                {
                    FILE_LOG( logERROR ) << "[" << __func__ << "[FindCalibGrid::FindTargets]"
                                                               " Could not save result calib grid find to cache";
                    retVal = GC_ERR;
                }
            }
#ifdef DEBUG_FIND_CALIB_GRID   // output template matches to CSV file
            FILE *fp = fopen( ( DEBUG_RESULT_FOLDER + "matches.csv" ).c_str(), "w" );
            if ( nullptr != fp )
            {
                fprintf( fp, "Score, X, Y\n" );
                for ( size_t i = 0; i < m_matchItems.size(); ++i )
                {
                    fprintf( fp, "%.3f, %.3f, %.3f\n", m_matchItems[ i ].score,
                             m_matchItems[ i ].pt.x, m_matchItems[ i ].pt.y );
                }
                fclose( fp );
            }
#endif
        }
    }
    return retVal;
}
GC_STATUS FindCalibGrid::MatchRefine( const int index, const Mat &img, const double minScore,
                                      const int numToFind, TemplateBowtieItem &item )
{
    GC_STATUS retVal = GC_OK;

    if ( 0 > index || TEMPLATE_COUNT <= index )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::MatchRefine]"
                                                   " Attempted to find template index=" << index << \
                                                   " Must be in range 0-" << TEMPLATE_COUNT - 1;
        retVal = GC_ERR;
    }
    else if ( 0.05 > minScore || 1.0 < minScore )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::MatchRefine]"
                                                   " Min score %.3f must be in range 0.05-1.0" << minScore;
        retVal = GC_ERR;
    }
    else if ( 1 > numToFind || 1000 < numToFind )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::MatchRefine]"
                                                   " Attempted to find " << numToFind << \
                                                   " matches.  Must be in range 1-1000";
        retVal = GC_ERR;
    }
    else
    {
        try
        {
            Rect rect;
            double minScore, maxScore;
            Point ptMin, ptMax;
            Point2d ptFinal;

            m_matchSpace = 0;
            TemplateBowtieItem itemTemp;

            matchTemplate( img, m_templates[ static_cast< size_t >( index ) ], m_matchSpace, TM_CCOEFF_NORMED );
#ifdef DEBUG_FIND_CALIB_GRID
            Mat matTemp;
            normalize( m_matchSpace, matTemp, 255.0 );
            imwrite( DEBUG_RESULT_FOLDER + "bowtie_match_fine.png", matTemp );
#endif
            rect.x = std::max( 0, cvRound( item.pt.x )- ( m_templates[ 0 ].cols >> 1 ) - ( m_templates[ 0 ].cols >> 2 ) );
            rect.y = std::max( 0, cvRound( item.pt.y ) - ( m_templates[ 0 ].rows >> 1 ) - ( m_templates[ 0 ].rows >> 2 ) );
            rect.width = m_templates[ 0 ].cols + ( m_templates[ 0 ].cols >> 1 );
            rect.height = m_templates[ 0 ].rows + ( m_templates[ 0 ].rows >> 1 );
            if ( rect.x + rect.width >= img.cols )
                rect.x = img.cols - rect.width;
            if ( rect.y + rect.height >= img.rows )
                rect.y = img.rows - rect.height;

            Mat matROI = img( rect );
            m_matchSpaceSmall = 0;

            matchTemplate( matROI, m_templates[ static_cast< size_t >( index ) ], m_matchSpaceSmall, TM_CCOEFF_NORMED );
            minMaxLoc( m_matchSpaceSmall, &minScore, &maxScore, &ptMin, &ptMax );
            if ( maxScore > item.score )
            {
                retVal = SubpixelPointRefine( m_matchSpaceSmall, ptMax, ptFinal );
                if ( GC_OK == retVal )
                {
                    // ptFinal = Point2d( static_cast< double >( ptMax.x ), static_cast< double >( ptMax.y ) );
                    item.score = maxScore;
                    item.pt.x = static_cast< double >( rect.x ) + ptFinal.x + static_cast< double >( m_templates[ 0 ].cols ) / 2.0;
                    item.pt.y = static_cast< double >( rect.y ) + ptFinal.y + static_cast< double >( m_templates[ 0 ].rows ) / 2.0;
                }
            }
        }
        catch( exception &e )
        {
            FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::MatchRefine] " << e.what();
            retVal = GC_EXCEPT;
        }
    }
    return retVal;
}
GC_STATUS FindCalibGrid::MatchTemplate( const int index, const Mat &img, const double minScore, const int numToFind )
{
    GC_STATUS retVal = GC_OK;

    if ( 0 > index || TEMPLATE_COUNT <= index )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::MatchTemplate]"
                                " Attempted to find template index=" << index << \
                                " Must be in range 0-" << TEMPLATE_COUNT - 1;
        retVal = GC_ERR;
    }
    else if ( 0.05 > minScore || 1.0 < minScore )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::MatchTemplate]"
                                " Min score %.3f must be in range 0.05-1.0" << minScore;
        retVal = GC_ERR;
    }
    else if ( 1 > numToFind || 1000 < numToFind )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::MatchTemplate]"
                                                   " Attempted to find " << numToFind << \
                                                   " matches.  Must be in range 1-1000";
        retVal = GC_ERR;
    }
    else
    {
        try
        {
            Rect rect;
            double dMin, dMax;
            Point ptMin, ptMax;
            Point2d ptFinal;
            TemplateBowtieItem itemTemp;

            m_matchSpace = 0.0;
            m_matchItems.clear();
            matchTemplate( img, m_templates[ static_cast< size_t >( index ) ], m_matchSpace, cv::TM_CCOEFF_NORMED );

#ifdef DEBUG_FIND_CALIB_GRID
            Mat matTemp;
            normalize( m_matchSpace, matTemp, 255.0 );
            imwrite( DEBUG_RESULT_FOLDER + "bowtie_match_coarse.png", matTemp );
#endif

            for ( int i = 0; i < numToFind; i++ )
            {
                minMaxLoc( m_matchSpace, &dMin, &dMax, &ptMin, &ptMax );
                if (  0 < ptMax.x && 0 < ptMax.y && img.cols - 1 > ptMax.x && img.rows - 1 > ptMax.y )
                {
                    if ( dMax >= minScore )
                    {
                        itemTemp.score = dMax;
                        itemTemp.pt.x = static_cast< double >( ptMax.x ) + static_cast< double >( m_templates[ 0 ].cols ) / 2.0;
                        itemTemp.pt.y = static_cast< double >( ptMax.y ) + static_cast< double >( m_templates[ 0 ].rows ) / 2.0;
                        m_matchItems.push_back( itemTemp );
                    }
                    else
                        break;
                }
                circle( m_matchSpace, ptMax, 17, Scalar( 0.0 ), FILLED );
            }
        }
        catch( exception &e )
        {
            FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::MatchTemplate] " << e.what();
            return GC_EXCEPT;
        }
        if ( m_matchItems.empty() )
        {
            FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::MatchTemplate] No template matches found";
            retVal = GC_ERR;
        }
    }
    return retVal;
}
GC_STATUS FindCalibGrid::GetFoundPoints( vector< vector< Point2d > > &pts )
{
    GC_STATUS retVal = GC_OK;
    if ( m_itemArray.empty() )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::GetFoundPoints]"
                                " No points available in found points array";
        retVal = GC_ERR;
    }
    else if ( CALIB_POINT_COL_COUNT * CALIB_POINT_ROW_COUNT != m_itemArray[ 0 ].size() * m_itemArray.size() )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::GetFoundPoints]"
                                " Invalid found points array " << m_itemArray[ 0 ].size() << "x" << \
                                m_itemArray.size() << " should be " << CALIB_POINT_COL_COUNT << "x" << CALIB_POINT_ROW_COUNT;
        retVal = GC_ERR;
    }
    else
    {
        pts.clear();
        vector< Point2d > temp;
        for ( size_t i = 0; i < m_itemArray.size(); ++i )
        {
            temp.clear();
            for ( size_t j = 0; j < m_itemArray[ i ].size(); ++j )
            {
                temp.push_back( m_itemArray[ i ][ j ].pt );
            }
            pts.push_back( temp );
        }
    }
    return retVal;
}
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// helper methods
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
GC_STATUS FindCalibGrid::SortPoints( const Size sizeSearchImage )
{
    GC_STATUS retVal = GC_OK;

    size_t bowtieCount = CALIB_POINT_ROW_COUNT * CALIB_POINT_COL_COUNT;
    if ( bowtieCount > m_matchItems.size() )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::SortPoints] Invalid found point count="
                             << m_matchItems.size() << " --  Should be at least " << bowtieCount;
        retVal = GC_ERR;
    }
    else
    {
        try
        {
            vector< TemplateBowtieItem > tempItems;
            for ( size_t i = 0; i < bowtieCount; ++i )
                tempItems.push_back( m_matchItems[ i ] );

            // sort by score (high scores at the top)
            sort( tempItems.begin(), tempItems.end(), []( TemplateBowtieItem const &a, TemplateBowtieItem const &b )
            {
                return ( a.score > b.score );
            } );

            m_matchItems.clear();
            for ( size_t i = 0; i < bowtieCount; ++i )
                m_matchItems.push_back( tempItems[ i ] );

            // sort by y
            sort( m_matchItems.begin(), m_matchItems.end(), []( TemplateBowtieItem const &a, TemplateBowtieItem const &b )
            {
                return ( a.pt.y < b.pt.y );
            } );

            m_itemArray.clear();
            for( size_t i = 0; i < CALIB_POINT_ROW_COUNT; ++i )
            {
                tempItems.clear();
                for( size_t j = 0; j < CALIB_POINT_COL_COUNT; ++j )
                {
                    tempItems.push_back( m_matchItems[ i * CALIB_POINT_COL_COUNT + j ] );
                }
                sort( tempItems.begin(), tempItems.end(), []( TemplateBowtieItem const &a, TemplateBowtieItem const &b )
                {
                    return ( a.pt.x < b.pt.x );
                } );
                m_itemArray.push_back( tempItems );
            }

            int searchDim = cvRound( m_itemArray[ 0 ][ CALIB_POINT_COL_COUNT - 1 ].pt.y - m_itemArray[ 0 ][ 0 ].pt.y );
            m_rectLeftMoveSearch.x = MAX( 0, cvRound( m_itemArray[ 0 ][ 0 ].pt.x ) - ( searchDim >> 1 ) );
            m_rectLeftMoveSearch.y = MAX( 0, cvRound( m_itemArray[ 0 ][ 0 ].pt.y ) - ( searchDim >> 1 ) );
            m_rectLeftMoveSearch.width = searchDim;
            m_rectLeftMoveSearch.height = searchDim;
            m_rectRightMoveSearch.x = cvRound( m_itemArray[ 0 ][ CALIB_POINT_COL_COUNT - 1 ].pt.x ) - ( searchDim >> 1 );
            m_rectRightMoveSearch.y = MAX( 0, cvRound( m_itemArray[ 0 ][ CALIB_POINT_COL_COUNT - 1 ].pt.y ) - ( searchDim >> 1 ) );
            m_rectRightMoveSearch.height = searchDim;
            m_rectRightMoveSearch.width = searchDim + m_rectRightMoveSearch.x > sizeSearchImage.width ?
                        sizeSearchImage.width - m_rectRightMoveSearch.x - 1 : searchDim;
        }
        catch( exception &e )
        {
            FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::SortPoints] " << e.what();
            return GC_EXCEPT;
        }
    }

    return retVal;
}
GC_STATUS FindCalibGrid::SubpixelPointRefine( const Mat &matchSpace, const Point ptMax, Point2d &ptResult )
{
    GC_STATUS retVal = GC_OK;
    if ( 1 > ptMax.x || 1 > ptMax.y || matchSpace.cols - 2 < ptMax.x || matchSpace.rows - 2 < ptMax.y )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::SubpixelPointRefine]"
                                " Invalid point (not on image) for subpixel refinement";
        retVal = GC_ERR;
    }
    else if ( CV_32FC1 != matchSpace.type() )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::SubpixelPointRefine]"
                                " Invalid image format for subpixel refinement";
        retVal = GC_ERR;
    }
    else
    {
        try
        {
            float val;
            int col, row;
            float total = 0.0f;
            float totalX = 0.0f;
            float totalY = 0.0f;
            for ( row = ptMax.y - 1; row < ptMax.y + 2; ++row )
            {
                for ( col = ptMax.x - 1; col < ptMax.x + 2; ++col )
                {
                    val = matchSpace.at< float >( Point( col, row ) );
                    total += val;
                    totalX += val * static_cast< float >( col );
                    totalY += val * static_cast< float >( row );
                }
            }
            ptResult = Point2d( static_cast< double >( totalX / total ),
                                static_cast< double >( totalY / total ) );
        }
        catch( exception &e )
        {
            FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::SubpixelPointRefine] " << e.what();
            retVal = GC_EXCEPT;
        }
    }
    return retVal;
}
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// movement methods
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
GC_STATUS FindCalibGrid::SetMoveTargetROI( const Mat &img, const Rect rect, const bool isLeft )
{
    GC_STATUS retVal = GC_OK;
    if ( rect.x < 0 || rect.y < 0 || rect.x + rect.width > img.cols || rect.y + rect.height > img.rows )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::SetMoveTargetROI]"
                                " Invalid " << ( isLeft ? "left" : "right" ) << "search ROI dimension";
        retVal = GC_ERR;
    }
    if ( isLeft )
        m_rectLeftMoveSearch = rect;
    else
        m_rectRightMoveSearch = rect;
    return retVal;
}
void FindCalibGrid::GetMoveTargetROIs( Rect &rectLeft, Rect &rectRight )
{
    rectLeft = m_rectLeftMoveSearch;
    rectRight = m_rectRightMoveSearch;
}
GC_STATUS FindCalibGrid::FindMoveTargets( const Mat &img, Point2d &ptLeft, Point2d &ptRight )
{
    GC_STATUS retVal = GC_OK;
    if ( m_templates.empty() )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::FindMoveTargets]"
                                 " Cannot find move targets in an uninitialized object";
        retVal = GC_ERR;
    }
    else if ( img.empty() )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::FindMoveTargets]"
                                " Cannot find move targets in an empty image";
        retVal = GC_ERR;
    }
    else
    {
        try
        {
            Mat scratch( img.size(), CV_8UC1 );
            scratch = 0;

            Mat matMoveSearch = img( m_rectLeftMoveSearch );
            Mat matMoveSearchScratch = scratch( m_rectLeftMoveSearch );
            matMoveSearch.copyTo( matMoveSearchScratch );

            matMoveSearch = img( m_rectRightMoveSearch );
            matMoveSearchScratch = scratch( m_rectRightMoveSearch );
            matMoveSearch.copyTo( matMoveSearchScratch );

#ifdef DEBUG_FIND_CALIB_GRID
            imwrite( DEBUG_RESULT_FOLDER + "move_search_image.png", scratch );
#endif

            m_matchItems.clear();

            retVal = MatchTemplate( TEMPLATE_COUNT >> 1, scratch, TEMPLATE_MATCH_MIN_SCORE, 2 );
            if ( GC_OK == retVal )
            {
                vector< TemplateBowtieItem > tempItems;
                for ( size_t i = 0; i < m_matchItems.size(); ++i )
                    tempItems.push_back( m_matchItems[ i ] );

                m_matchItems.clear();
                for ( size_t i = 0; i < tempItems.size(); ++i )
                {
                    for ( int j = 0; j < TEMPLATE_COUNT; ++j )
                    {
                        retVal = MatchRefine( j, scratch, 0.5, 1, tempItems[ i ] );
                        if ( GC_OK != retVal )
                            break;
                    }
                    if ( GC_OK != retVal )
                        break;
                    m_matchItems.push_back( tempItems[ i ] );
                }
            }
            if ( 2 != m_matchItems.size() )
            {
                FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::FindMoveTargets]"
                                        " Invalid move point count=" << m_matchItems.size() << ".  Should be 2";
                retVal = GC_ERR;
            }
            else
            {
                if ( m_matchItems[ 0 ].pt.x < m_matchItems[ 1 ].pt.x )
                {
                    ptLeft =  m_matchItems[ 0 ].pt;
                    ptRight =  m_matchItems[ 1 ].pt;
                }
                else
                {
                    ptLeft =  m_matchItems[ 1 ].pt;
                    ptRight =  m_matchItems[ 0 ].pt;
                }
            }
        }
        catch( Exception &e )
        {
            FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::FindMoveTargets] " << e.what();
            retVal = GC_EXCEPT;
        }
    }

    return retVal;
}
GC_STATUS FindCalibGrid::DrawMoveROIs( Mat &img )
{
    GC_STATUS retVal = GC_OK;
    if ( CV_8UC1 != img.depth() )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::DrawMoveROIs]"
                                " Invalid image format for drawing move search ROI's";
        retVal = GC_ERR;
    }
    else if ( m_rectLeftMoveSearch.x < 0 || m_rectLeftMoveSearch.y < 0 ||
              m_rectLeftMoveSearch.x + m_rectLeftMoveSearch.width > img.cols ||
              m_rectLeftMoveSearch.y + m_rectLeftMoveSearch.height > img.rows ||
              m_rectRightMoveSearch.x < 0 || m_rectRightMoveSearch.y < 0 ||
              m_rectRightMoveSearch.x + m_rectRightMoveSearch.width > img.cols ||
              m_rectRightMoveSearch.y + m_rectRightMoveSearch.height > img.rows )
    {
        FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::DrawMoveROIs]"
                                " Invalid search ROI dimension for move ROI drawing";
        retVal = GC_ERR;
    }
    else
    {
        try
        {
            rectangle( img, Point( m_rectLeftMoveSearch.x, m_rectLeftMoveSearch.y ),
                               Point( m_rectLeftMoveSearch.x + m_rectLeftMoveSearch.width,
                                        m_rectLeftMoveSearch.y + m_rectLeftMoveSearch.height ),
                         Scalar( 0, 0, 255 ), 2 );
            rectangle( img, Point( m_rectRightMoveSearch.x, m_rectRightMoveSearch.y ),
                               Point( m_rectRightMoveSearch.x + m_rectRightMoveSearch.width,
                                        m_rectRightMoveSearch.y + m_rectRightMoveSearch.height ),
                         Scalar( 0, 0, 255 ), 2 );
        }
        catch( exception &e )
        {
            FILE_LOG( logERROR ) << "[" << __func__ << "][FindCalibGrid::DrawMoveROIs] " << e.what();
            retVal = GC_EXCEPT;
        }
    }

    return retVal;
}

} // namespace gc
