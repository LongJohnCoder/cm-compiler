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

#include "Scan2FilePipeline.h"
#include "CmdParser.h"

using namespace std;

int main(int argc, char* argv[])
{
   if (!ParseCommandLine(argc, argv))
      return 0;

   Scan2FilePipeline *scanpipeline = new Scan2FilePipeline();

   scanpipeline->VAInit();

   if (!FLAGS_flowpath.compare("encode"))
   {
      if (scanpipeline->EncodeInit(FLAGS_i.c_str(), FLAGS_width, FLAGS_height,
               FLAGS_yuvformat) < 0)
      {
         throw std::runtime_error(std::string("JPEG Encoder initialization error"));
      }

      scanpipeline->AssemblerCompressGraph(FLAGS_jpegquality);
      scanpipeline->ExecuteCompressGraph(FLAGS_maxframes);

      if (!FLAGS_jpegout.empty())
      {
         scanpipeline->Save2JPEG(FLAGS_jpegout.c_str());
      }
   }
   else if (!FLAGS_flowpath.compare("decode"))
   {
      if (scanpipeline->DecodeInit(FLAGS_i.c_str()) < 0)
      {
         throw std::runtime_error(std::string("JPEG Decoder initialization error"));
      }
      scanpipeline->AssemblerDecompressGraph();
      scanpipeline->ExecuteDecompressGraph(FLAGS_maxframes);

      if (!FLAGS_rawout.empty())
      {
         scanpipeline->Save2Raw(FLAGS_rawout.c_str());
      }
   }

	return 0;
}
