/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include <gflags/gflags.h>

// @brief message for help argument
static const char help_message[] = "Print a usage message.";

// @brief message for input image argument
static const char input_image_message[] = "Path to an 24bit .raw image";

// @brief message for JPEG output image argument
static const char jpeg_output_image_message[] = "Path to save JPEG output image. Default: no output file";

// @brief message for RGB raw output image argument
static const char raw_output_image_message[] = "Path to save RGB raw output image. Default: no output file";

// @brief message for max-frames
static const char max_frames_message[] = "Maximum number of frames to run.";

// @brief message for jpeg-quality
static const char jpeg_quality_message[] = "JPEG compression quality, range(1, 100). Default value: 90";

// @brief message for yuv format
static const char yuv_format_message[] = "YUV format, Option 0:YUV444, 1: Y8 (Grayscale). Default: 0: YUV444";

// @brief message for image width
static const char width_message[] = "Input image width";

// @brief message for image height
static const char height_message[] = "Input image height";

// @brief message for tile-height
static const char tile_height_message[] = "Tile height, Height need to divide evenly with the tile height .  Default value: 32";

// @brief message for path
static const char flowpath_message[] = "encode (For JPEG Encode function); decode (For JPEG Decode function); encdec (For encode, then decode)  Default value: encode";


// @brief Define flag for showing help message
DEFINE_bool(h, false, help_message);

// @brief Define parameter for input image file
DEFINE_string(i, "nest.bmp", input_image_message);

// @brief Define parameter for jpeg output image file
DEFINE_string(jpegout, "", jpeg_output_image_message);

// @brief Define parameter for output image file
DEFINE_string(rawout, "", raw_output_image_message);

// @brief Define parameter for maximum number of frames to run
// Default is 10
DEFINE_int32(maxframes, 1, max_frames_message);

// @brief Define parameter for maximum number of frames to run
// Default is 90
DEFINE_int32(jpegquality, 90, jpeg_quality_message);

// @brief Define parameter for YUV format
// Default is YUV444
DEFINE_int32(yuvformat, 0, yuv_format_message);

// @brief message for image width
DEFINE_int32(width, 0, width_message);

// @brief message for image height
DEFINE_int32(height, 0, height_message);

// @brief Define parameter for Tile Height setting
// Default is 32 rows
DEFINE_int32(tileheight, 0, tile_height_message);

// @brief Define parameter for output image file
DEFINE_string(flowpath, "encode", flowpath_message);


static void showUsage() {
   std::cout << std::endl;
   std::cout << "hw_x64.ScanToJpeg [OPTION]" << std::endl;
   std::cout << "Options:" << std::endl;
   std::cout << std::endl;
   std::cout << "   -h                          " << help_message << std::endl;
   std::cout << "   -i <filename>               " << input_image_message << std::endl;
   std::cout << "   --width <integer>           " << width_message << std::endl;
   std::cout << "   --height <integer>          " << height_message << std::endl;
   std::cout << "   --flowpath <type>           " << flowpath_message << std::endl;
   std::cout << "   --jpegout <filename>        " << jpeg_output_image_message << std::endl;
   std::cout << "   --rawout <filename>         " << raw_output_image_message << std::endl;
   std::cout << "   --maxframes <integer>       " << max_frames_message <<  std::endl;
   std::cout << "   --jpegquality <integer>     " << jpeg_quality_message <<  std::endl;
   std::cout << "   --yuvformat <integer>       " <<  yuv_format_message <<  std::endl;
   std::cout << "   --tileheight <integer>      " << tile_height_message <<  std::endl;
   std::cout << std::endl;
   std::cout << std::endl;
}

bool ParseCommandLine(int argc, char *argv[])
{
   gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
   if (FLAGS_h) {
      showUsage();
      return false;
   }

   if (!FLAGS_flowpath.compare("encode"))
   {
      if ((FLAGS_width == 0) || (FLAGS_height == 0)) {
         throw std::logic_error("Please enter a valid image width or height");
      }

      if (FLAGS_width > 32 * 1024) {
         throw std::out_of_range("Maximum width supported is 32768 by this sample");
      }

      if ((FLAGS_width > 16 * 1024) && (FLAGS_width / 2 % 8)) {
         throw std::out_of_range("Detected width > 16K, but width/2 is not divisible by 8");
      }
   }

   if ((FLAGS_yuvformat < 0) || (FLAGS_yuvformat > 1)) {
      throw std::logic_error("Only support yuvformat 0 or 1");
   }

   if (FLAGS_i.empty()) {
      throw std::logic_error("Parameter -i is not set");
   }

   if ((FLAGS_tileheight != 0) && (FLAGS_tileheight % 8)) {
        throw std::out_of_range("TileHeight need to divisible by 8");
   } else if (FLAGS_tileheight == 0) {
      FLAGS_tileheight = FLAGS_height;
   }

   return true;
}
