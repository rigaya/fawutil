// -----------------------------------------------------------------------------------------
// QSVEnc/NVEnc by rigaya
// -----------------------------------------------------------------------------------------
// The MIT License
//
// Copyright (c) 2023 rigaya
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// --------------------------------------------------------------------------------------------

#ifndef __FAW_VERSION_H__
#define __FAW_VERSION_H__

#define VER_FILEVERSION             0,0,1,0
#define VER_STR_FILEVERSION          "0.01"
#define VER_STR_FILEVERSION_TCHAR _T("0.01")

#ifdef DEBUG
#define VER_DEBUG   VS_FF_DEBUG
#define VER_PRIVATE VS_FF_PRIVATEBUILD
#else
#define VER_DEBUG   0
#define VER_PRIVATE 0
#endif

#ifdef _M_IX86
#define FAWUTIL_FILENAME "fawutil (x86)"
#else
#define FAWUTIL_FILENAME "fawutil (x64)"
#endif

#define VER_STR_COMMENTS         "fawutil"
#define VER_STR_COMPANYNAME      ""
#define VER_STR_FILEDESCRIPTION  FAWUTIL_FILENAME
#define VER_STR_INTERNALNAME     FAWUTIL_FILENAME
#define VER_STR_ORIGINALFILENAME "fawutil.exe"
#define VER_STR_LEGALCOPYRIGHT   "fawutil by rigaya"
#define VER_STR_PRODUCTNAME      FAWUTIL_FILENAME
#define VER_PRODUCTVERSION       VER_FILEVERSION
#define VER_STR_PRODUCTVERSION   VER_STR_FILEVERSION

#endif //__FAW_VERSION_H__
