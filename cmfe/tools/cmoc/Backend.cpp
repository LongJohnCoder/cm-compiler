/*===================== begin_copyright_notice ==================================

 Copyright (c) 2020, Intel Corporation


 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
======================= end_copyright_notice ==================================*/


#include "Common.h"

#ifdef USE_OCLOC_API_HEADER
#include "ocloc_api.h"
#endif

#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <string>
#include <unordered_map>

#include <cassert>

namespace {
struct LibOclocWrapper {
#ifdef USE_OCLOC_API_HEADER
  using invokePtr = decltype(::oclocInvoke) *;
  using freeOutputPtr = decltype(::oclocFreeOutput) *;
#else
  using invokePtr = int (*)(unsigned, const char **, const uint32_t,
                            const uint8_t **, const uint64_t *, const char **,
                            const uint32_t, const uint8_t **, const uint64_t *,
                            const char **, uint32_t *, uint8_t ***, uint64_t **,
                            char ***);
  using freeOutputPtr = int (*)(uint32_t *, uint8_t ***, uint64_t **, char ***);
#endif

  invokePtr invoke;
  freeOutputPtr freeOutput;

  LibOclocWrapper() {
    const std::string LibPath = getLibOclocName();
    std::string Err;
    auto Lib =
        llvm::sys::DynamicLibrary::getPermanentLibrary(LibPath.c_str(), &Err);
    if (!Lib.isValid())
      FatalError(Err);

    invoke = reinterpret_cast<invokePtr>(Lib.getAddressOfSymbol("oclocInvoke"));
    if (!invoke)
      FatalError("oclocInvoke symbol is missing");
    freeOutput = reinterpret_cast<freeOutputPtr>(
        Lib.getAddressOfSymbol("oclocFreeOutput"));
    if (!freeOutput)
      FatalError("oclocFreeOutput symbol is missing");
  }

private:
  static std::string getLibOclocName() {
    std::string Dir;
    if (auto EnvDir = llvm::sys::Process::GetEnv("CMOC_OCLOC_DIR"))
      Dir = EnvDir.getValue();
    llvm::SmallString<32> Path;
    llvm::sys::path::append(Path, Dir, LIBOCLOC_NAME);
    return Path.str().str();
  }
};
} // namespace

// clang-format off
const std::unordered_map<std::string, std::string> CmToNeoCPU{
    {"SKL", "skl"},
    {"ICLLP", "icllp"},
    {"TGLLP", "tgllp"},
};
// clang-format on

static std::string translateCPU(const std::string &CPU) {
  auto It = CmToNeoCPU.find(CPU);
  assert(It != CmToNeoCPU.end() && "Unexpected CPU model");
  return It->second;
}

// Try to guess revision id for given stepping.
// Relies on enumeration values inside driver since
// ocloc accepts these numeric values instead of letter codes.
static std::string translateStepping(const std::string &CPU) {
  enum RevIdMap { A = 0, B = 3 };
  return "";
}

template <typename OsT>
static void printEscapedString(OsT &OS, const char *Str, const char Escape) {
  OS << Escape;
  while (char C = *Str++)
    (C == Escape ? OS << '\\' : OS) << C;
  OS << Escape;
}

// Print strings wrapping each with Escape characters and prepending
// '\' to each of escape charaters inside strings.
template <typename OsT>
static void printEscapedArgs(OsT &OS, const std::vector<const char *> &Args,
                             const char Escape) {
  for (auto *Arg : Args) {
    printEscapedString(OS, Arg, Escape);
    OS << ' ';
  }
  OS << '\n';
}

// Copy outputs. We are interested only in .gen and .dbg.
// gen -- direct output of IGC. dbg -- debug info.
// Gen binary should always be present, debug info is optional.
static void saveOutputs(uint32_t NumOutputs, uint8_t **DataOutputs,
                        uint64_t *LenOutputs, char **NameOutputs,
                        ILTranslationResult &Result) {
  llvm::ArrayRef<const uint8_t *> OutBins{DataOutputs, NumOutputs};
  llvm::ArrayRef<uint64_t> OutLens{LenOutputs, NumOutputs};
  llvm::ArrayRef<const char *> OutNames{NameOutputs, NumOutputs};
  auto Zip = llvm::zip(OutBins, OutLens, OutNames);
  using ZipTy = typename decltype(Zip)::value_type;
  auto BinIt = std::find_if(Zip.begin(), Zip.end(), [](ZipTy File) {
    llvm::StringRef Name{std::get<2>(File)};
    return Name.endswith(".gen");
  });
  auto DbgIt = std::find_if(Zip.begin(), Zip.end(), [](ZipTy File) {
    llvm::StringRef Name{std::get<2>(File)};
    return Name.endswith(".dbg");
  });
  assert(BinIt != Zip.end() && "Gen binary is missing");

  llvm::ArrayRef<uint8_t> BinRef{std::get<0>(*BinIt),
                                 static_cast<std::size_t>(std::get<1>(*BinIt))};
  Result.KernelBinary.assign(BinRef.begin(), BinRef.end());
  if (DbgIt != Zip.end()) {
    llvm::ArrayRef<uint8_t> DbgRef{
        std::get<0>(*DbgIt), static_cast<std::size_t>(std::get<1>(*DbgIt))};
    Result.DebugInfo.assign(DbgRef.begin(), DbgRef.end());
  }
}

static void invokeBE(const std::vector<char> &SPIRV, const std::string &NeoCPU,
                     const std::string &RevId, const std::string &Options,
                     const std::string &InternalOptions,
                     ILTranslationResult &Result) {
  const LibOclocWrapper LibOcloc;

  const char *SpvFileName = "cmoc_spirv";

  std::vector<const char *> OclocArgs;
  OclocArgs.push_back("ocloc");
  OclocArgs.push_back("compile");
  OclocArgs.push_back("-device");
  OclocArgs.push_back(NeoCPU.c_str());
  if (!RevId.empty()) {
    OclocArgs.push_back("-revision_id");
    OclocArgs.push_back(RevId.c_str());
  }
  OclocArgs.push_back("-spirv_input");
  OclocArgs.push_back("-file");
  OclocArgs.push_back(SpvFileName);
  OclocArgs.push_back("-options");
  OclocArgs.push_back(Options.c_str());
  OclocArgs.push_back("-internal_options");
  OclocArgs.push_back(InternalOptions.c_str());

  if (isCmocDebugEnabled()) {
    llvm::errs() << "oclocInvoke options: ";
    printEscapedArgs(llvm::errs(), OclocArgs, '"');
  }

  // Silence ocloc if no debug.
  if (!isCmocDebugEnabled())
    OclocArgs.push_back("-q");

  uint32_t NumOutputs = 0;
  uint8_t **DataOutputs = nullptr;
  uint64_t *LenOutputs = nullptr;
  char **NameOutputs = nullptr;

  static_assert(alignof(uint8_t) == alignof(char), "Possible unaligned access");
  auto *SpvSource = reinterpret_cast<const uint8_t *>(SPIRV.data());
  const uint64_t SpvLen = SPIRV.size();
  if (LibOcloc.invoke(OclocArgs.size(), OclocArgs.data(), 1, &SpvSource,
                      &SpvLen, &SpvFileName, 0, nullptr, nullptr, nullptr,
                      &NumOutputs, &DataOutputs, &LenOutputs, &NameOutputs))
    FatalError("Call to oclocInvoke failed");
  saveOutputs(NumOutputs, DataOutputs, LenOutputs, NameOutputs, Result);
  if (LibOcloc.freeOutput(&NumOutputs, &DataOutputs, &LenOutputs, &NameOutputs))
    FatalError("Call to oclocFreeOutput failed");
}

static std::string composeOptions() {
  std::string Options{"-vc-codegen "};

  auto AuxVCOptions = llvm::sys::Process::GetEnv("CM_VC_API_OPTIONS");
  if (AuxVCOptions) {
    Options.append(" ").append(AuxVCOptions.getValue());
  }

  return Options;
}

static std::string
composeInternalOptions(const std::string &BinFormat,
                       const std::vector<std::string> &BackendOptions,
                       const std::string &Features, bool TimePasses) {
  std::string InternalOptions;
  InternalOptions.append(" -binary-format=").append(BinFormat);

  if (!BackendOptions.empty())
    InternalOptions +=
        " -llvm-options='" + llvm::join(BackendOptions, " ") + "'";

  if (!Features.empty())
    InternalOptions.append(" -target-features=").append(Features);

  if (auto EnvInternalOptions =
          llvm::sys::Process::GetEnv("CM_INTERNAL_OPTIONS"))
    InternalOptions.append(" ").append(EnvInternalOptions.getValue());

  if (TimePasses)
    InternalOptions.append(" -ftime-report");

  return InternalOptions;
}

void translateIL(const std::string &CPUName, const std::string &BinFormat,
                 const std::string &Features,
                 const std::vector<std::string> &BackendOptions,
                 const std::vector<char> &SPIRV_IR, InputKind IK,
                 bool TimePasses, ILTranslationResult &Result) {

  if (isCmocDebugEnabled()) {
    llvm::errs() << "requested platform for translateIL: " << CPUName << "\n";
    llvm::errs() << "requested runtime for translateIL: " << BinFormat << "\n";
  }

  const std::string Options = composeOptions();
  const std::string InternalOptions =
      composeInternalOptions(BinFormat, BackendOptions, Features, TimePasses);

  if (isCmocDebugEnabled()) {
    llvm::errs() << "IGC Translation Options: " << Options << "\n";
    llvm::errs() << "IGC Translation Internal: " << InternalOptions << "\n";
  }

  const std::string NeoCPU = translateCPU(CPUName);
  const std::string RevId = translateStepping(CPUName);

  invokeBE(SPIRV_IR, NeoCPU, RevId, Options, InternalOptions, Result);
}
