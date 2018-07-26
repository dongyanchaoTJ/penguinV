#include "image_function_opencl.h"

#include <math.h>
#include "../thirdparty/multicl/src/opencl_device.h"
#include "../thirdparty/multicl/src/opencl_helper.h"
#include "../image_function_helper.h"
#include "../parameter_validation.h"

namespace
{
    const std::string programCode = R"(
        #pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

        __kernel void absoluteDifferenceOpenCL( __global const unsigned char * in1, unsigned int rowSizeIn1, __global const unsigned char * in2, unsigned int rowSizeIn2,
                                                __global unsigned char * out, unsigned int rowSizeOut, unsigned int width, unsigned int height )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height ) {
                const size_t idIn1 = y * rowSizeIn1 + x;
                const size_t idIn2 = y * rowSizeIn2 + x;
                const size_t idOut = y * rowSizeOut + x;
                out[idOut] = (in1[idIn1] > in2[idIn2]) ? (in1[idIn1] - in2[idIn2]) : (in2[idIn2] - in1[idIn1]);
            }
        }

        __kernel void bitwiseAndOpenCL( __global const unsigned char * in1, unsigned int rowSizeIn1, __global const unsigned char * in2, unsigned int rowSizeIn2,
                                        __global unsigned char * out, unsigned int rowSizeOut, unsigned int width, unsigned int height )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height ) {
                const size_t idIn1 = y * rowSizeIn1 + x;
                const size_t idIn2 = y * rowSizeIn2 + x;
                const size_t idOut = y * rowSizeOut + x;
                out[idOut] = (in1[idIn1] & in2[idIn2]);
            }
        }

        __kernel void bitwiseOrOpenCL( __global const unsigned char * in1, unsigned int rowSizeIn1, __global const unsigned char * in2, unsigned int rowSizeIn2,
                                       __global unsigned char * out, unsigned int rowSizeOut, unsigned int width, unsigned int height )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height ) {
                const size_t idIn1 = y * rowSizeIn1 + x;
                const size_t idIn2 = y * rowSizeIn2 + x;
                const size_t idOut = y * rowSizeOut + x;
                out[idOut] = (in1[idIn1] | in2[idIn2]);
            }
        }

        __kernel void bitwiseXorOpenCL( __global const unsigned char * in1, unsigned int rowSizeIn1, __global const unsigned char * in2, unsigned int rowSizeIn2,
                                        __global unsigned char * out, unsigned int rowSizeOut, unsigned int width, unsigned int height )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height ) {
                const size_t idIn1 = y * rowSizeIn1 + x;
                const size_t idIn2 = y * rowSizeIn2 + x;
                const size_t idOut = y * rowSizeOut + x;
                out[idOut] = (in1[idIn1] ^ in2[idIn2]);
            }
        }

        __kernel void convertToGrayScaleOpenCL( __global const unsigned char * in, unsigned int rowSizeIn, unsigned char colorCount, __global unsigned char * out, unsigned int rowSizeOut,
                                                unsigned int width, unsigned int height )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height ) {
                __global const unsigned char * data = in + (rowSizeIn * y) + x * colorCount;

                unsigned int sum = 0;
                for( unsigned char i = 0; i < colorCount; ++i, ++data )
                {
                    sum += (*data);
                }

                const size_t id = y * rowSizeOut + x;
                out[id] = sum / colorCount;
            }
        }

        __kernel void convertToRgbOpenCL( __global const unsigned char * in, unsigned int rowSizeIn, __global unsigned char * out, unsigned int rowSizeOut, unsigned char colorCount,
                                          unsigned int width, unsigned int height )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height ) {
                const size_t id = y * rowSizeIn + x;

                __global unsigned char * data = out + (rowSizeOut * y) + x * colorCount;

                for( unsigned char i = 0; i < colorCount; ++i, ++data )
                {
                    (*data) = in[id];
                }
            }
        }

        __kernel void copyOpenCL( __global const unsigned char * in, unsigned int rowSizeIn, __global unsigned char * out, unsigned int rowSizeOut, unsigned int width, unsigned int height )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if ( x < width && y < height ) {
                out[y * rowSizeOut + x] = in[y * rowSizeIn + x];
            }
        }

        __kernel void extractChannelOpenCL( __global const unsigned char * in, unsigned int rowSizeIn, unsigned char colorCount, __global unsigned char * out, unsigned int rowSizeOut,
                                            unsigned int width, unsigned int height, unsigned char channelId )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height )
                out[y * rowSizeOut + x] = in[(y * rowSizeIn + x) * colorCount + channelId];
        }

        __kernel void fillOpenCL( __global unsigned char * data, unsigned int rowSize, unsigned int width, unsigned int height, unsigned char value )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height )
                data[y * rowSize + x] = value;
        }

        __kernel void flipOpenCL( __global const unsigned char * in, unsigned int rowSizeIn, __global unsigned char * out, unsigned int rowSizeOut,
                                  unsigned int width, unsigned int height, unsigned char horizontal, unsigned char vertical )
        {
            const size_t inX = get_global_id(0);
            const size_t inY = get_global_id(1);

            if( inX < width && inY < height ) {
                const size_t outX = (horizontal != 0) ? (width  - 1 - inX) : inX;
                const size_t outY = (vertical != 0)   ? (height - 1 - inY) : inY;

                out[outY * rowSizeOut + outX] = in[inY * rowSizeIn + inX];
            }
        }

        __kernel void histogramOpenCL( __global const unsigned char * data, unsigned int rowSize, unsigned int width, unsigned int height, volatile __global unsigned int * histogram )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height ) {
                const size_t id = y * rowSize + x;
                atomic_add( &histogram[data[id]], 1 );
            }
        }

        __kernel void invertOpenCL( __global const unsigned char * in, unsigned int rowSizeIn, __global unsigned char * out, unsigned int rowSizeOut, unsigned int width, unsigned int height )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height )
                out[y * rowSizeOut + x] = ~in[y * rowSizeIn + x];
        }

        __kernel void lookupTableOpenCL( __global const unsigned char * in, unsigned int rowSizeIn, __global unsigned char * out, unsigned int rowSizeOut,
                                         unsigned int width, unsigned int height, __global unsigned char * table )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height )
                out[y * rowSizeOut + x] = table[in[y * rowSizeIn + x]];
        }

        __kernel void maximumOpenCL( __global const unsigned char * in1, unsigned int rowSizeIn1, __global const unsigned char * in2, unsigned int rowSizeIn2,
                                     __global unsigned char * out, unsigned int rowSizeOut, unsigned int width, unsigned int height )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height ) {
                const size_t idIn1 = y * rowSizeIn1 + x;
                const size_t idIn2 = y * rowSizeIn2 + x;
                const size_t idOut = y * rowSizeOut + x;
                out[idOut] = (in1[idIn1] > in2[idIn2]) ? in1[idIn1] : in2[idIn2];
            }
        }

        __kernel void minimumOpenCL( __global const unsigned char * in1, unsigned int rowSizeIn1, __global const unsigned char * in2, unsigned int rowSizeIn2,
                                     __global unsigned char * out, unsigned int rowSizeOut, unsigned int width, unsigned int height )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height ) {
                const size_t idIn1 = y * rowSizeIn1 + x;
                const size_t idIn2 = y * rowSizeIn2 + x;
                const size_t idOut = y * rowSizeOut + x;
                out[idOut] = (in1[idIn1] < in2[idIn2]) ? in1[idIn1] : in2[idIn2];
            }
        }

        __kernel void subtractOpenCL( __global const unsigned char * in1, unsigned int rowSizeIn1, __global const unsigned char * in2, unsigned int rowSizeIn2,
                                      __global unsigned char * out, unsigned int rowSizeOut, unsigned int width, unsigned int height )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height ) {
                const size_t idIn1 = y * rowSizeIn1 + x;
                const size_t idIn2 = y * rowSizeIn2 + x;
                const size_t idOut = y * rowSizeOut + x;
                out[idOut] = (in1[idIn1] > in2[idIn2]) ? (in1[idIn1] - in2[idIn2]) : 0;
            }
        }

        __kernel void thresholdOpenCL( __global const unsigned char * in, unsigned int rowSizeIn, __global unsigned char * out, unsigned int rowSizeOut,
                                       unsigned int width, unsigned int height, unsigned char threshold )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height ) {
                out[y * rowSizeOut + x] = (in[y * rowSizeIn + x] < threshold) ? 0 : 255;
            }
        }

        __kernel void thresholdDoubleOpenCL( __global const unsigned char * in, unsigned int rowSizeIn, __global unsigned char * out, unsigned int rowSizeOut,
                                             unsigned int width, unsigned int height, unsigned char minThreshold, unsigned char maxThreshold )
        {
            const size_t x = get_global_id(0);
            const size_t y = get_global_id(1);

            if( x < width && y < height ) {
                const size_t idIn = y * rowSizeIn + x;
                out[y * rowSizeOut + x] = ((in[idIn] < minThreshold) || (in[idIn] > maxThreshold)) ? 0 : 255;
            }
        }
        )";

    multiCL::OpenCLProgram GetProgram()
    {
        return multiCL::OpenCLProgram( multiCL::OpenCLDeviceManager::instance().device().context(), programCode );
    }
}

namespace Image_Function_OpenCL
{
    Image AbsoluteDifference( const Image & in1, const Image & in2 )
    {
        return Image_Function_Helper::AbsoluteDifference( AbsoluteDifference, in1, in2 );
    }

    void AbsoluteDifference( const Image & in1, const Image & in2, Image & out )
    {
        Image_Function_Helper::AbsoluteDifference( AbsoluteDifference, in1, in2, out );
    }

    Image AbsoluteDifference( const Image & in1, uint32_t startX1, uint32_t startY1, const Image & in2, uint32_t startX2, uint32_t startY2,
                              uint32_t width, uint32_t height )
    {
        return Image_Function_Helper::AbsoluteDifference( AbsoluteDifference, in1, startX1, startY1, in2, startX2, startY2, width, height );
    }

    void AbsoluteDifference( const Image & in1, uint32_t startX1, uint32_t startY1, const Image & in2, uint32_t startX2, uint32_t startY2,
                             Image & out, uint32_t startXOut, uint32_t startYOut, uint32_t width, uint32_t height )
    {
        Image_Function::ParameterValidation( in1, startX1, startY1, in2, startX2, startY2, out, startXOut, startYOut, width, height );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "absoluteDifferenceOpenCL");
        kernel.setArgument( in1.data(), in1.rowSize(), in2.data(), in2.rowSize(), out.data(), out.rowSize(), width, height );

        multiCL::launchKernel2D( kernel, width, height );
    }

    Image BitwiseAnd( const Image & in1, const Image & in2 )
    {
        return Image_Function_Helper::BitwiseAnd( BitwiseAnd, in1, in2 );
    }

    void BitwiseAnd( const Image & in1, const Image & in2, Image & out )
    {
        Image_Function_Helper::BitwiseAnd( BitwiseAnd, in1, in2, out );
    }

    Image BitwiseAnd( const Image & in1, uint32_t startX1, uint32_t startY1, const Image & in2, uint32_t startX2, uint32_t startY2,
                      uint32_t width, uint32_t height )
    {
        return Image_Function_Helper::BitwiseAnd( BitwiseAnd, in1, startX1, startY1, in2, startX2, startY2, width, height );
    }

    void BitwiseAnd( const Image & in1, uint32_t startX1, uint32_t startY1, const Image & in2, uint32_t startX2, uint32_t startY2,
                     Image & out, uint32_t startXOut, uint32_t startYOut, uint32_t width, uint32_t height )
    {
        Image_Function::ParameterValidation( in1, startX1, startY1, in2, startX2, startY2, out, startXOut, startYOut, width, height );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "bitwiseAndOpenCL");
        kernel.setArgument( in1.data(), in1.rowSize(), in2.data(), in2.rowSize(), out.data(), out.rowSize(), width, height );

        multiCL::launchKernel2D( kernel, width, height );
    }

    Image BitwiseOr( const Image & in1, const Image & in2 )
    {
        return Image_Function_Helper::BitwiseOr( BitwiseOr, in1, in2 );
    }

    void BitwiseOr( const Image & in1, const Image & in2, Image & out )
    {
        Image_Function_Helper::BitwiseOr( BitwiseOr, in1, in2, out );
    }

    Image BitwiseOr( const Image & in1, uint32_t startX1, uint32_t startY1, const Image & in2, uint32_t startX2, uint32_t startY2,
                     uint32_t width, uint32_t height )
    {
        return Image_Function_Helper::BitwiseOr( BitwiseOr, in1, startX1, startY1, in2, startX2, startY2, width, height );
    }

    void BitwiseOr( const Image & in1, uint32_t startX1, uint32_t startY1, const Image & in2, uint32_t startX2, uint32_t startY2,
                    Image & out, uint32_t startXOut, uint32_t startYOut, uint32_t width, uint32_t height )
    {
        Image_Function::ParameterValidation( in1, startX1, startY1, in2, startX2, startY2, out, startXOut, startYOut, width, height );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "bitwiseOrOpenCL");
        kernel.setArgument( in1.data(), in1.rowSize(), in2.data(), in2.rowSize(), out.data(), out.rowSize(), width, height );

        multiCL::launchKernel2D( kernel, width, height );
    }

    Image BitwiseXor( const Image & in1, const Image & in2 )
    {
        return Image_Function_Helper::BitwiseXor( BitwiseXor, in1, in2 );
    }

    void BitwiseXor( const Image & in1, const Image & in2, Image & out )
    {
        Image_Function_Helper::BitwiseXor( BitwiseXor, in1, in2, out );
    }

    Image BitwiseXor( const Image & in1, uint32_t startX1, uint32_t startY1, const Image & in2, uint32_t startX2, uint32_t startY2,
                      uint32_t width, uint32_t height )
    {
        return Image_Function_Helper::BitwiseXor( BitwiseXor, in1, startX1, startY1, in2, startX2, startY2, width, height );
    }

    void BitwiseXor( const Image & in1, uint32_t startX1, uint32_t startY1, const Image & in2, uint32_t startX2, uint32_t startY2,
                     Image & out, uint32_t startXOut, uint32_t startYOut, uint32_t width, uint32_t height )
    {
        Image_Function::ParameterValidation( in1, startX1, startY1, in2, startX2, startY2, out, startXOut, startYOut, width, height );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "bitwiseXorOpenCL");
        kernel.setArgument( in1.data(), in1.rowSize(), in2.data(), in2.rowSize(), out.data(), out.rowSize(), width, height );

        multiCL::launchKernel2D( kernel, width, height );
    }

    ImageOpenCL ConvertToOpenCL( const PenguinV_Image::Image & in )
    {
        ImageOpenCL out( in.width(), in.height(), in.colorCount() );

        ConvertToOpenCL( in, out );

        return out;
    }

    void ConvertToOpenCL( const Image & in, Image & out )
    {
        Image_Function::ParameterValidation( in, out );

        if( in.width() != out.width() || in.height() != out.height() ||
            in.colorCount() != out.colorCount())
            throw imageException( "Bad input parameters in image function" );

        if (in.alignment() == 1u || (in.rowSize() == in.width() * in.colorCount()))
        {
            const size_t size = in.rowSize() * in.height();
            multiCL::writeBuffer( reinterpret_cast<cl_mem>( out.data() ), size * sizeof(uint8_t), in.data() );
        }
        else
        {
            const size_t origin[3] = {0, 0, 0};
            const size_t region[3] = {in.width(), in.height(), 1};

            multiCL::openCLCheck( clEnqueueWriteImage( multiCL::OpenCLDeviceManager::instance().device().queue()(), reinterpret_cast<cl_mem>( out.data() ), CL_TRUE,
                                                       origin, region, in.rowSize(), 0, in.data(), 0, NULL, NULL ) );
        }
    }

    Image ConvertFromOpenCL( const Image & in )
    {
        Image out( in.width(), in.height(), in.colorCount() );

        ConvertFromOpenCL( in, out );

        return out;
    }

    void ConvertFromOpenCL( const Image & in, Image & out )
    {
        Image_Function::ParameterValidation( in, out );

        if( in.width() != out.width() || in.height() != out.height() ||
            in.colorCount() != out.colorCount())
            throw imageException( "Bad input parameters in image function" );

        if (out.alignment() == 1u || (out.rowSize() == out.width() * out.colorCount()))
        {
            const size_t size = in.rowSize() * in.height();
            multiCL::readBuffer( reinterpret_cast<cl_mem>( const_cast<uint8_t*>( in.data() ) ), size * sizeof(uint8_t), out.data() );
        }
        else
        {
            const size_t origin[3] = {0, 0, 0};
            const size_t region[3] = {out.width(), out.height(), 1};

            multiCL::openCLCheck( clEnqueueReadImage( multiCL::OpenCLDeviceManager::instance().device().queue()(), reinterpret_cast<cl_mem>( const_cast<uint8_t*>( in.data() ) ),
                                                      CL_TRUE, origin, region, out.rowSize(), 0, out.data(), 0, NULL, NULL ) );
        }
    }

    Image ConvertToGrayScale( const Image & in )
    {
        return Image_Function_Helper::ConvertToGrayScale( ConvertToGrayScale, in );
    }

    void ConvertToGrayScale( const Image & in, Image & out )
    {
        Image_Function_Helper::ConvertToGrayScale( ConvertToGrayScale, in, out );
    }

    Image ConvertToGrayScale( const Image & in, uint32_t startXIn, uint32_t startYIn, uint32_t width, uint32_t height )
    {
        return Image_Function_Helper::ConvertToGrayScale( ConvertToGrayScale, in, startXIn, startYIn, width, height );
    }

    void ConvertToGrayScale( const Image & in, uint32_t startXIn, uint32_t startYIn, Image & out, uint32_t startXOut, uint32_t startYOut,
                             uint32_t width, uint32_t height )
    {
        Image_Function::ParameterValidation( in, startXIn, startYIn, out, startXOut, startYOut, width, height );
        Image_Function::VerifyGrayScaleImage( out );

        if( in.colorCount() == PenguinV_Image::GRAY_SCALE ) {
            Copy( in, out );
            return;
        }

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "convertToGrayScaleOpenCL");
        kernel.setArgument( in.data(), in.rowSize(), in.colorCount(), out.data(), out.rowSize(), width, height );

        multiCL::launchKernel2D( kernel, width, height );
    }

    Image ConvertToRgb( const Image & in )
    {
        return Image_Function_Helper::ConvertToRgb( ConvertToRgb, in );
    }

    void ConvertToRgb( const Image & in, Image & out )
    {
        Image_Function_Helper::ConvertToRgb( ConvertToRgb, in, out );
    }

    Image ConvertToRgb( const Image & in, uint32_t startXIn, uint32_t startYIn, uint32_t width, uint32_t height )
    {
        return Image_Function_Helper::ConvertToRgb( ConvertToRgb, in, startXIn, startYIn, width, height );
    }

    void ConvertToRgb( const Image & in, uint32_t startXIn, uint32_t startYIn, Image & out, uint32_t startXOut, uint32_t startYOut,
                       uint32_t width, uint32_t height )
    {
        Image_Function::ParameterValidation( in, startXIn, startYIn, out, startXOut, startYOut, width, height );
        Image_Function::VerifyRGBImage     ( out );

        if( in.colorCount() == PenguinV_Image::RGB ) {
            Copy( in, out );
        }
        else {
            const multiCL::OpenCLProgram & program = GetProgram();
            multiCL::OpenCLKernel kernel( program, "convertToRgbOpenCL");
            kernel.setArgument( in.data(), in.rowSize(), out.data(), out.rowSize(), out.colorCount(), width, height );

            multiCL::launchKernel2D( kernel, width, height );
        }
    }

    void Copy( const Image & in, Image & out )
    {
        Image_Function::ParameterValidation( in, out );

        out = in;
    }

    Image Copy( const Image & in, uint32_t startXIn, uint32_t startYIn, uint32_t width, uint32_t height )
    {
        return Image_Function_Helper::Copy( Copy, in, startXIn, startYIn, width, height );
    }

    void Copy( const Image & in, uint32_t startXIn, uint32_t startYIn, Image & out, uint32_t startXOut, uint32_t startYOut,
               uint32_t width, uint32_t height )
    {
        Image_Function::ParameterValidation( in, startXIn, startYIn, out, startXOut, startYOut, width, height );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "copyOpenCL");
        kernel.setArgument( in.data(), in.rowSize(), out.data(), out.rowSize(), width, height );

        multiCL::launchKernel2D( kernel, width, height );
    }

    Image ExtractChannel( const Image & in, uint8_t channelId )
    {
        return Image_Function_Helper::ExtractChannel( ExtractChannel, in, channelId );
    }

    void ExtractChannel( const Image & in, Image & out, uint8_t channelId )
    {
        Image_Function_Helper::ExtractChannel( ExtractChannel, in, out, channelId );
    }

    Image ExtractChannel( const Image & in, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t channelId )
    {
        return Image_Function_Helper::ExtractChannel( ExtractChannel, in, x, y, width, height, channelId );
    }

    void ExtractChannel( const Image & in, uint32_t startXIn, uint32_t startYIn, Image & out, uint32_t startXOut,
                         uint32_t startYOut, uint32_t width, uint32_t height, uint8_t channelId )
    {
        Image_Function::ParameterValidation( in, startXIn, startYIn, out, startXOut, startYOut, width, height );
        Image_Function::VerifyGrayScaleImage( out );

        if( channelId >= in.colorCount() )
            throw imageException( "Channel ID for color image is greater than channel count in input image" );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "extractChannelOpenCL");
        kernel.setArgument( in.data(), in.data(), in.colorCount(), out.data(), out.rowSize(), width, height, channelId );

        multiCL::launchKernel2D( kernel, width, height );
    }

    void Fill( Image & image, uint8_t value )
    {
        image.fill( value );
    }

    void Fill( Image & image, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t value )
    {
        Image_Function::ParameterValidation( image, x, y, width, height );
        Image_Function::VerifyGrayScaleImage( image );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "fillOpenCL");
        kernel.setArgument( image.data(), image.rowSize(), width, height, value );

        multiCL::launchKernel2D( kernel, width, height );
    }

    Image Flip( const Image & in, bool horizontal, bool vertical )
    {
        Image_Function::ParameterValidation( in );

        Image out = in.generate( in.width(), in.height(), in.colorCount(), in.alignment() );

        Flip( in, out, horizontal, vertical );

        return out;
    }

    void Flip( const Image & in, Image & out, bool horizontal, bool vertical )
    {
        Image_Function::ParameterValidation( in, out );
        Image_Function::VerifyGrayScaleImage( in, out );

        if( !horizontal && !vertical ) {
            Copy( in, out );
        }
        else {
            const multiCL::OpenCLProgram & program = GetProgram();
            multiCL::OpenCLKernel kernel( program, "flipOpenCL");
            kernel.setArgument( in.data(), in.rowSize(), out.data(), out.rowSize(), in.width(), in.height(), horizontal, vertical );

            multiCL::launchKernel2D( kernel, in.width(), in.height() );
        }
    }

    Image GammaCorrection( const Image & in, double a, double gamma )
    {
        return Image_Function_Helper::GammaCorrection( GammaCorrection, in, a, gamma );
    }

    void GammaCorrection( const Image & in, Image & out, double a, double gamma )
    {
        Image_Function_Helper::GammaCorrection( GammaCorrection, in, out, a, gamma );
    }

    Image GammaCorrection( const Image & in, uint32_t startXIn, uint32_t startYIn, uint32_t width, uint32_t height, double a, double gamma )
    {
        return Image_Function_Helper::GammaCorrection( GammaCorrection, in, startXIn, startYIn, width, height, a, gamma );
    }

    void GammaCorrection( const Image & in, uint32_t startXIn, uint32_t startYIn, Image & out, uint32_t startXOut, uint32_t startYOut,
                          uint32_t width, uint32_t height, double a, double gamma )
    {
        Image_Function::ParameterValidation( in, startXIn, startYIn, out, startXOut, startYOut, width, height );
        Image_Function::VerifyGrayScaleImage( in, out );

        if( a < 0 || gamma < 0 )
            throw imageException( "Bad input parameters in image function" );

        // We precalculate all values and store them in lookup table
        std::vector < uint8_t > value( 256 );

        for( uint16_t i = 0; i < 256; ++i ) {
            double data = a * pow( i / 255.0, gamma ) * 255 + 0.5;

            if( data < 256 )
                value[i] = static_cast<uint8_t>(data);
            else
                value[i] = 255;
        }

        LookupTable( in, startXIn, startYIn, out, startXOut, startYOut, width, height, value );
    }

    uint8_t GetThreshold( const std::vector < uint32_t > & histogram )
    {
        return Image_Function_Helper::GetThreshold( histogram );
    }

    std::vector < uint32_t > Histogram( const Image & image )
    {
        return Image_Function_Helper::Histogram( Histogram, image );
    }

    void Histogram( const Image & image, std::vector < uint32_t > & histogram )
    {
        Image_Function_Helper::Histogram( Histogram, image, histogram );
    }

    std::vector < uint32_t > Histogram( const Image & image, uint32_t x, uint32_t y, uint32_t width, uint32_t height )
    {
        return Image_Function_Helper::Histogram( Histogram, image, x, y, width, height );
    }

    void Histogram( const Image & image, uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::vector < uint32_t > & histogram )
    {
        Image_Function::ParameterValidation( image, x, y, width, height );
        Image_Function::VerifyGrayScaleImage( image );

        histogram.resize( 256u );
        std::fill( histogram.begin(), histogram.end(), 0u );

        cl_mem histogramOpenCL = multiCL::MemoryManager::memory().allocate<uint32_t>( histogram.size() );
        multiCL::writeBuffer( histogramOpenCL, sizeof( uint32_t ) * histogram.size(), histogram.data() );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "histogramOpenCL");
        kernel.setArgument( image.data(), image.rowSize(), width, height, histogramOpenCL );

        multiCL::launchKernel2D( kernel, width, height );

        multiCL::readBuffer( histogramOpenCL, sizeof( uint32_t ) * histogram.size(), histogram.data() );
        multiCL::MemoryManager::memory().free( histogramOpenCL );
    }

    Image Invert( const Image & in )
    {
        return Image_Function_Helper::Invert( Invert, in );
    }

    void Invert( const Image & in, Image & out )
    {
        Image_Function_Helper::Invert( Invert, in, out );
    }

    Image Invert( const Image & in, uint32_t startXIn, uint32_t startYIn, uint32_t width, uint32_t height )
    {
        return Image_Function_Helper::Invert( Invert, in, startXIn, startYIn, width, height );
    }

    void Invert( const Image & in, uint32_t startXIn, uint32_t startYIn, Image & out, uint32_t startXOut, uint32_t startYOut,
                 uint32_t width, uint32_t height )
    {
        Image_Function::ParameterValidation( in, startXIn, startYIn, out, startXOut, startYOut, width, height );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "invertOpenCL");
        kernel.setArgument( in.data(), in.rowSize(), out.data(), out.rowSize(), width, height );

        multiCL::launchKernel2D( kernel, width, height );
    }

    Image LookupTable( const Image & in, const std::vector < uint8_t > & table )
    {
        return Image_Function_Helper::LookupTable( LookupTable, in, table );
    }

    void LookupTable( const Image & in, Image & out, const std::vector < uint8_t > & table )
    {
        Image_Function_Helper::LookupTable( LookupTable, in, out, table );
    }

    Image LookupTable( const Image & in, uint32_t startXIn, uint32_t startYIn, uint32_t width, uint32_t height,
                       const std::vector < uint8_t > & table )
    {
        return Image_Function_Helper::LookupTable( LookupTable, in, startXIn, startYIn, width, height, table );
    }

    void LookupTable( const Image & in, uint32_t startXIn, uint32_t startYIn, Image & out, uint32_t startXOut, uint32_t startYOut,
                      uint32_t width, uint32_t height, const std::vector < uint8_t > & table )
    {
        Image_Function::ParameterValidation( in, startXIn, startYIn, out, startXOut, startYOut, width, height );
        Image_Function::VerifyGrayScaleImage( in, out );

        if( table.size() != 256u )
            throw imageException( "Lookup table size is not equal to 256" );

        cl_mem tableOpenCL = multiCL::MemoryManager::memory().allocate<uint8_t>( table.size() );
        multiCL::writeBuffer( tableOpenCL, sizeof(uint8_t) * table.size(), table.data() );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "lookupTableOpenCL");
        kernel.setArgument( in.data(), in.rowSize(), out.data(), out.rowSize(), width, height, tableOpenCL );

        multiCL::launchKernel2D( kernel, width, height );

        multiCL::MemoryManager::memory().free( tableOpenCL );
    }

    Image Maximum( const Image & in1, const Image & in2 )
    {
        return Image_Function_Helper::Maximum( Maximum, in1, in2 );
    }

    void Maximum( const Image & in1, const Image & in2, Image & out )
    {
        Image_Function_Helper::Maximum( Maximum, in1, in2, out );
    }

    Image Maximum( const Image & in1, uint32_t startX1, uint32_t startY1, const Image & in2, uint32_t startX2, uint32_t startY2,
                   uint32_t width, uint32_t height )
    {
        return Image_Function_Helper::Maximum( Maximum, in1, startX1, startY1, in2, startX2, startY2, width, height );
    }

    void Maximum( const Image & in1, uint32_t startX1, uint32_t startY1, const Image & in2, uint32_t startX2, uint32_t startY2,
                  Image & out, uint32_t startXOut, uint32_t startYOut, uint32_t width, uint32_t height )
    {
        Image_Function::ParameterValidation( in1, startX1, startY1, in2, startX2, startY2, out, startXOut, startYOut, width, height );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "maximumOpenCL");
        kernel.setArgument( in1.data(), in1.rowSize(), in2.data(), in2.rowSize(), out.data(), out.rowSize(), width, height );

        multiCL::launchKernel2D( kernel, width, height );
    }

    Image Minimum( const Image & in1, const Image & in2 )
    {
        return Image_Function_Helper::Minimum( Minimum, in1, in2 );
    }

    void Minimum( const Image & in1, const Image & in2, Image & out )
    {
        Image_Function_Helper::Minimum( Minimum, in1, in2, out );
    }

    Image Minimum( const Image & in1, uint32_t startX1, uint32_t startY1, const Image & in2, uint32_t startX2, uint32_t startY2,
                   uint32_t width, uint32_t height )
    {
        return Image_Function_Helper::Minimum( Minimum, in1, startX1, startY1, in2, startX2, startY2, width, height );
    }

    void Minimum( const Image & in1, uint32_t startX1, uint32_t startY1, const Image & in2, uint32_t startX2, uint32_t startY2,
                  Image & out, uint32_t startXOut, uint32_t startYOut, uint32_t width, uint32_t height )
    {
        Image_Function::ParameterValidation( in1, startX1, startY1, in2, startX2, startY2, out, startXOut, startYOut, width, height );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "minimumOpenCL");
        kernel.setArgument( in1.data(), in1.rowSize(), in2.data(), in2.rowSize(), out.data(), out.rowSize(), width, height );

        multiCL::launchKernel2D( kernel, width, height );
    }

    Image Subtract( const Image & in1, const Image & in2 )
    {
        return Image_Function_Helper::Subtract( Subtract, in1, in2 );
    }

    void Subtract( const Image & in1, const Image & in2, Image & out )
    {
        Image_Function_Helper::Subtract( Subtract, in1, in2, out );
    }

    Image Subtract( const Image & in1, uint32_t startX1, uint32_t startY1, const Image & in2, uint32_t startX2, uint32_t startY2,
                    uint32_t width, uint32_t height )
    {
        return Image_Function_Helper::Subtract( Subtract, in1, startX1, startY1, in2, startX2, startY2, width, height );
    }

    void Subtract( const Image & in1, uint32_t startX1, uint32_t startY1, const Image & in2, uint32_t startX2, uint32_t startY2,
                   Image & out, uint32_t startXOut, uint32_t startYOut, uint32_t width, uint32_t height )
    {
        Image_Function::ParameterValidation( in1, startX1, startY1, in2, startX2, startY2, out, startXOut, startYOut, width, height );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "subtractOpenCL");
        kernel.setArgument( in1.data(), in1.rowSize(), in2.data(), in2.rowSize(), out.data(), out.rowSize(), width, height );

        multiCL::launchKernel2D( kernel, width, height );
    }

    Image Threshold( const Image & in, uint8_t threshold )
    {
        return Image_Function_Helper::Threshold( Threshold, in, threshold );
    }

    void Threshold( const Image & in, Image & out, uint8_t threshold )
    {
        Image_Function_Helper::Threshold( Threshold, in, out, threshold );
    }

    Image Threshold( const Image & in, uint32_t startXIn, uint32_t startYIn, uint32_t width, uint32_t height, uint8_t threshold )
    {
        return Image_Function_Helper::Threshold( Threshold, in, startXIn, startYIn, width, height, threshold );
    }

    void Threshold( const Image & in, uint32_t startXIn, uint32_t startYIn, Image & out, uint32_t startXOut, uint32_t startYOut,
                    uint32_t width, uint32_t height, uint8_t threshold )
    {
        Image_Function::ParameterValidation( in, startXIn, startYIn, out, startXOut, startYOut, width, height );
        Image_Function::VerifyGrayScaleImage( in, out );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "thresholdOpenCL");
        kernel.setArgument( in.data(), in.rowSize(), out.data(), out.rowSize(), width, height, threshold );

        multiCL::launchKernel2D( kernel, width, height );
    }

    Image Threshold( const Image & in, uint8_t minThreshold, uint8_t maxThreshold )
    {
        return Image_Function_Helper::Threshold( Threshold, in, minThreshold, maxThreshold );
    }

    void Threshold( const Image & in, Image & out, uint8_t minThreshold, uint8_t maxThreshold )
    {
        Image_Function_Helper::Threshold( Threshold, in, out, minThreshold, maxThreshold );
    }

    Image Threshold( const Image & in, uint32_t startXIn, uint32_t startYIn, uint32_t width, uint32_t height, uint8_t minThreshold,
                     uint8_t maxThreshold )
    {
        return Image_Function_Helper::Threshold( Threshold, in, startXIn, startYIn, width, height, minThreshold, maxThreshold );
    }

    void Threshold( const Image & in, uint32_t startXIn, uint32_t startYIn, Image & out, uint32_t startXOut, uint32_t startYOut,
                    uint32_t width, uint32_t height, uint8_t minThreshold, uint8_t maxThreshold )
    {
        Image_Function::ParameterValidation( in, startXIn, startYIn, out, startXOut, startYOut, width, height );
        Image_Function::VerifyGrayScaleImage( in, out );

        const multiCL::OpenCLProgram & program = GetProgram();
        multiCL::OpenCLKernel kernel( program, "thresholdDoubleOpenCL");
        kernel.setArgument( in.data(), in.rowSize(), out.data(), out.rowSize(), width, height, minThreshold, maxThreshold );

        multiCL::launchKernel2D( kernel, width, height );
    }
}