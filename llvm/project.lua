local sys = require "build.sys"
local Twine = require "Twine"
local List = require "List"

local llvm = sys.getLoadingProject()

llvm.is_external = true

local mode = sys.cfg.llvm and sys.cfg.llvm.mode or "Release"

local llvm_builddir = "llvm_build/"..mode

lake.mkdir(llvm_builddir, {make_parents = true})

llvm.report.dir.include 
{
  -- Include these folders directly as opposed to trying to reorganize them
  -- elsewhere as llvm *probably* does some crazy stuff that breaks simply 
  -- hard linking to .h files. Maybe soft linking would work? Idk I dont even
  -- have linking setup yet so come back to this when I do.
  direct = 
  {
    llvm.root.."/src/clang/include",
    llvm.root.."/src/llvm/include",
    llvm.root.."/"..llvm_builddir.."/include",
    llvm.root.."/"..llvm_builddir.."/tools/clang/include",
  }
}

-- NOTE(sushi) these are NOT in a proper order for linking! 
--             Currently I am just cheating and using --start/end-group to
--             avoid having to figure out the proper ordering for all of these
--             libs. From the 10-15 seconds I took to look at MSVC's link 
--             options it doesnt seem like they have an equivalent, so I will 
--             need to take time to figure this out.. probably just involves 
--             looking at the makefiles that CMake generates for clang or 
--             maybe clang-check or whatever.
-- This list can be generated from llvm/getlibs.sh
local libs = Twine.new
  "LLVMBPFCodeGen"
  "LLVMDlltoolDriver"
  "LLVMExecutionEngine"
  "clangAnalysisFlowSensitiveModels"
  "clangCodeGen"
  "lldMachO"
  "LLVMFuzzMutate"
  "LLVMLanaiInfo"
  "LLVMExegesisAArch64"
  "LLVMAsmPrinter"
  "LLVMInstCombine"
  "clangToolingASTDiff"
  "LLVMBPFDesc"
  "LLVMAMDGPUTargetMCA"
  "LLVMPowerPCCodeGen"
  "LLVMX86Disassembler"
  "clangToolingInclusions"
  "LLVMBPFDisassembler"
  "LLVMOrcShared"
  "LLVMMipsInfo"
  "LLVMBPFInfo"
  "LLVMObjCARCOpts"
  "LLVMX86TargetMCA"
  "LLVMARMInfo"
  "LLVMHexagonDisassembler"
  "LLVMLanaiCodeGen"
  "clangBasic"
  "LLVMipo"
  "clangStaticAnalyzerCheckers"
  "LLVMMSP430AsmParser"
  "clangIndexSerialization"
  "LLVMSymbolize"
  "LLVMAArch64Info"
  "LLVMWebAssemblyUtils"
  "LLVMAArch64Utils"
  "LLVMBinaryFormat"
  "LLVMMC"
  "LLVMDWARFLinkerParallel"
  "clangTransformer"
  "LLVMDebugInfoPDB"
  "LLVMRISCVAsmParser"
  "LLVMLTO"
  "LLVMOrcJIT"
  "LLVMCFGuard"
  "LLVMMipsAsmParser"
  "LLVMCoverage"
  "LLVMWebAssemblyDesc"
  "LLVMRISCVTargetMCA"
  "LLVMHexagonAsmParser"
  "LLVMLoongArchCodeGen"
  "LLVMMSP430Desc"
  "LLVMExegesis"
  "LLVMVectorize"
  "LLVMAMDGPUDisassembler"
  "clangSema"
  "LLVMDebugInfoCodeView"
  "LLVMX86CodeGen"
  "LLVMDebugInfoGSYM"
  "LLVMPowerPCAsmParser"
  "LLVMExegesisMips"
  "LLVMMipsCodeGen"
  "LLVMDemangle"
  "clangRewriteFrontend"
  "LLVMMIRParser"
  "LLVMMipsDisassembler"
  "LLVMARMDesc"
  "LLVMNVPTXInfo"
  -- "DynamicLibraryLib"
  "LLVMOption"
  "clangToolingInclusionsStdlib"
  "LLVMMCDisassembler"
  "LLVMMCJIT"
  "LLVMXCoreDisassembler"
  "LLVMOrcDebugging"
  "LLVMWindowsDriver"
  "clangFormat"
  "LLVMCGData"
  "LLVMLibDriver"
  "lldMinGW"
  "LLVMAArch64Disassembler"
  "LLVMXCoreDesc"
  "LLVMLineEditor"
  "LLVMFuzzerCLI"
  "LLVMRISCVDisassembler"
  "LLVMFrontendDriver"
  "clangInstallAPI"
  "LLVMBPFAsmParser"
  "LLVMPasses"
  "LLVMHexagonInfo"
  "LLVMFrontendHLSL"
  "LLVMAggressiveInstCombine"
  "LLVMExtensions"
  "LLVMRISCVDesc"
  "LLVMBitReader"
  "LLVMAMDGPUAsmParser"
  "LLVMNVPTXCodeGen"
  "clangDynamicASTMatchers"
  "LLVMGlobalISel"
  "LLVMARMAsmParser"
  "LLVMARMUtils"
  "LLVMXCoreCodeGen"
  "LLVMAVRCodeGen"
  "LLVMAArch64Desc"
  "LLVMDebugInfoDWARF"
  "LLVMIRPrinter"
  "LLVMAArch64CodeGen"
  "LLVMMipsDesc"
  "LLVMObjCopy"
  "LLVMWebAssemblyAsmParser"
  "LLVMCoroutines"
  "LLVMNVPTXDesc"
  "LLVMLoongArchAsmParser"
  "LLVMAVRAsmParser"
  "LLVMSparcDisassembler"
  "clangToolingCore"
  "LLVMAArch64AsmParser"
  "LLVMX86AsmParser"
  "LLVMAMDGPUCodeGen"
  "clangSerialization"
  "LLVMBitstreamReader"
  "LLVMWebAssemblyDisassembler"
  "LLVMCodeGenTypes"
  "clangInterpreter"
  "LLVMAsmParser"
  "LLVMTableGen"
  "clangDependencyScanning"
  "clangASTMatchers"
  "LLVMFrontendOpenMP"
  "LLVMDebuginfod"
  "clangFrontendTool"
  "LLVMSupport"
  "clangTooling"
  "clangToolingSyntax"
  "clangToolingRefactoring"
  "LLVMSystemZDesc"
  "LLVMSystemZInfo"
  "clangExtractAPI"
  "clangCrossTU"
  "LLVMFileCheck"
  "LLVMLoongArchDesc"
  "LLVMIRReader"
  "clangDirectoryWatcher"
  "LLVMCodeGen"
  "clangDriver"
  "LLVMLoongArchInfo"
  "LLVMVEAsmParser"
  "LLVMTargetParser"
  "LLVMAMDGPUDesc"
  "LLVMMCParser"
  "LLVMCFIVerify"
  "LLVMHipStdPar"
  "LLVMPowerPCInfo"
  "LLVMAVRInfo"
  "LLVMX86Info"
  "clangAPINotes"
  "LLVMExegesisX86"
  "LLVMDebugInfoBTF"
  "clangLex"
  "LLVMWindowsManifest"
  "LLVMSelectionDAG"
  "LLVMWebAssemblyInfo"
  "clangEdit"
  "LLVMSparcAsmParser"
  "lldCommon"
  "LLVMTextAPIBinaryReader"
  "lldWasm"
  "clangStaticAnalyzerCore"
  "LLVMDWP"
  "clangHandleLLVM"
  "LLVMPowerPCDesc"
  "LLVMMSP430Disassembler"
  "LLVMOptDriver"
  "LLVMSystemZAsmParser"
  "LLVMBitWriter"
  "clangStaticAnalyzerFrontend"
  "LLVMInstrumentation"
  "LLVMHexagonDesc"
  "LLVMRuntimeDyld"
  "clangAnalysisFlowSensitive"
  "clangFrontend"
  "clangAnalysis"
  "LLVMDWARFLinkerClassic"
  "LLVMLanaiAsmParser"
  "LLVMLanaiDisassembler"
  "LLVMXRay"
  "LLVMFrontendOffloading"
  "LLVMWebAssemblyCodeGen"
  "LLVMRISCVInfo"
  "LLVMInterfaceStub"
  "LLVMExegesisPowerPC"
  "clangIndex"
  "LLVMDWARFLinker"
  "LLVMObjectYAML"
  "LLVMSparcDesc"
  "clangAST"
  "LLVMProfileData"
  -- "LLVMTableGenCommon"
  "LLVMARMCodeGen"
  "LLVMVECodeGen"
  "LLVMPowerPCDisassembler"
  "LLVMLinker"
  "LLVMRISCVCodeGen"
  "LLVMAnalysis"
  "LLVMMSP430CodeGen"
  "LLVMSystemZCodeGen"
  "LLVMCore"
  "LLVMSandboxIR"
  "lldCOFF"
  "clangSupport"
  "clangParse"
  "LLVMVEDesc"
  "LLVMLoongArchDisassembler"
  "clangHandleCXX"
  "LLVMTableGenBasic"
  "lldELF"
  "LLVMVEDisassembler"
  "LLVMFrontendOpenACC"
  "LLVMDiff"
  "LLVMAMDGPUUtils"
  "LLVMObject"
  "LLVMXCoreInfo"
  "LLVMSparcCodeGen"
  "LLVMTextAPI"
  "LLVMScalarOpts"
  "LLVMOrcTargetProcess"
  "LLVMTarget"
  "LLVMARMDisassembler"
  "LLVMDebugInfoMSF"
  "LLVMSparcInfo"
  "LLVMHexagonCodeGen"
  "LLVMTransformUtils"
  "LLVMLanaiDesc"
  "LLVMRemarks"
  "LLVMSystemZDisassembler"
  "LLVMAVRDisassembler"
  "LLVMVEInfo"
  "LLVMMCA"
  "LLVMJITLink"
  "LLVMX86Desc"
  "clangRewrite"
  "LLVMDebugInfoLogicalView"
  "LLVMAMDGPUInfo"
  "LLVMAVRDesc"
  "LLVMInterpreter"
  "LLVMMSP430Info"
  "LLVMSPIRVAnalysis"
  "LLVMSPIRVCodeGen"
  "LLVMSPIRVDesc"
  "LLVMSPIRVInfo"
  "LLVMFrontendAtomic"

-- llvm.report.ext.pub.SharedLib "z"

for lib in libs:each() do
  llvm.report.ext.pub.StaticLib(lib)
end

llvm.report.dir.lib
{
  direct = 
  {
    llvm.root.."/"..llvm_builddir.."/lib"
  }
}

llvm.report.ext.pub.Exe(llvm_builddir.."/bin/clang++")
-- llvm.report.CMake(
--   llvm.root.."/src/llvm", 
--   "Unix Makefiles",
--   List
--   {
--     "-DLLVM_CCACHE_BUILD=ON",
--     "-DLLVM_OPTIMIZED_TABLEGEN=ON",
--     "-DLLVM_ENABLE_PROJECTS=clang;lld",
--     "-DCMAKE_BUILD_TYPE="..mode,
--     "-DLLVM_USE_LINKER=lld", -- TODO(sushi) support for other linkers
--     "-DLLVM_ENABLE_RUNTIMES=all",
--   },
--   llvm_builddir)
