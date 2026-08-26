#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace llvm;

namespace {

struct MutantRec {
    uint32_t id = 0;
    std::string file;
    unsigned line = 0;
    unsigned col = 0;
    std::string kind;
    std::string op;
    std::string mut;
};

static uint32_t fnv1a(StringRef s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h == 0 ? 1u : h;
}

static std::string jsonEscape(StringRef s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '"':
            o += "\\\"";
            break;
        case '\\':
            o += "\\\\";
            break;
        case '\n':
            o += "\\n";
            break;
        case '\r':
            o += "\\r";
            break;
        case '\t':
            o += "\\t";
            break;
        default:
            if (c < 0x20) {
                o += "\\u00";
                const char *hex = "0123456789abcdef";
                o.push_back(hex[c >> 4]);
                o.push_back(hex[c & 0xf]);
            } else {
                o.push_back(static_cast<char>(c));
            }
        }
    }
    return o;
}

static bool skipPath(StringRef p) {
    std::string lower = p.lower();
    if (lower.find("googletest") != std::string::npos ||
        lower.find("/gtest/") != std::string::npos ||
        lower.find("\\gtest\\") != std::string::npos ||
        lower.find("gtest_main") != std::string::npos ||
        lower.find("/gmock/") != std::string::npos) {
        return true;
    }
    if (p.ends_with("_test.cpp") || p.ends_with("_test.cc") || p.ends_with("_test.c") ||
        p.ends_with("_test.cxx") || p.ends_with("Test.cpp") || p.ends_with("_test.C")) {
        return true;
    }
    // System / toolchain headers (paths may be relative via ../ from clang).
    if (lower.find("/usr/include/") != std::string::npos ||
        lower.find("/usr/lib/") != std::string::npos || lower.find("/bits/") != std::string::npos ||
        lower.find("/lib/gcc/") != std::string::npos ||
        lower.find("include/c++/") != std::string::npos ||
        lower.find("include/c++\\") != std::string::npos ||
        lower.find("/libc++/") != std::string::npos) {
        return true;
    }
    return false;
}

static bool locOf(const Instruction *I, std::string &file, unsigned &line, unsigned &col) {
    const DebugLoc &DL = I->getDebugLoc();
    if (!DL) {
        if (const Function *F = I->getFunction()) {
            if (DISubprogram *SP = F->getSubprogram()) {
                file = SP->getFilename().str();
                StringRef dir = SP->getDirectory();
                if (!dir.empty() && !sys::path::is_absolute(file)) {
                    SmallString<256> full;
                    sys::path::append(full, dir, file);
                    file = full.str().str();
                }
                line = SP->getLine();
                col = 0;
                return line != 0 && !file.empty() && !skipPath(file);
            }
        }
        return false;
    }
    line = DL.getLine();
    col = DL.getCol();
    file.clear();
    if (DIScope *S = dyn_cast_or_null<DIScope>(DL.getScope())) {
        file = S->getFilename().str();
        StringRef dir = S->getDirectory();
        if (!dir.empty() && !sys::path::is_absolute(file)) {
            SmallString<256> full;
            sys::path::append(full, dir, file);
            file = full.str().str();
        }
    }
    return line != 0 && !file.empty() && !skipPath(file);
}

static const char *predName(CmpInst::Predicate P) {
    switch (P) {
    case CmpInst::ICMP_EQ:
        return "==";
    case CmpInst::ICMP_NE:
        return "!=";
    case CmpInst::ICMP_SGT:
        return ">";
    case CmpInst::ICMP_SGE:
        return ">=";
    case CmpInst::ICMP_SLT:
        return "<";
    case CmpInst::ICMP_SLE:
        return "<=";
    case CmpInst::ICMP_UGT:
        return ">";
    case CmpInst::ICMP_UGE:
        return ">=";
    case CmpInst::ICMP_ULT:
        return "<";
    case CmpInst::ICMP_ULE:
        return "<=";
    default:
        return "?";
    }
}

static bool pairPred(CmpInst::Predicate P, CmpInst::Predicate &Out) {
    switch (P) {
    case CmpInst::ICMP_EQ:
        Out = CmpInst::ICMP_NE;
        return true;
    case CmpInst::ICMP_NE:
        Out = CmpInst::ICMP_EQ;
        return true;
    case CmpInst::ICMP_SGT:
        Out = CmpInst::ICMP_SGE;
        return true;
    case CmpInst::ICMP_SGE:
        Out = CmpInst::ICMP_SGT;
        return true;
    case CmpInst::ICMP_SLT:
        Out = CmpInst::ICMP_SLE;
        return true;
    case CmpInst::ICMP_SLE:
        Out = CmpInst::ICMP_SLT;
        return true;
    case CmpInst::ICMP_UGT:
        Out = CmpInst::ICMP_UGE;
        return true;
    case CmpInst::ICMP_UGE:
        Out = CmpInst::ICMP_UGT;
        return true;
    case CmpInst::ICMP_ULT:
        Out = CmpInst::ICMP_ULE;
        return true;
    case CmpInst::ICMP_ULE:
        Out = CmpInst::ICMP_ULT;
        return true;
    default:
        return false;
    }
}

static const char *binName(Instruction::BinaryOps Op, Type *Ty) {
    const bool i1 = Ty && Ty->isIntegerTy(1);
    switch (Op) {
    case Instruction::Add:
        return "+";
    case Instruction::Sub:
        return "-";
    case Instruction::Mul:
        return "*";
    case Instruction::SDiv:
        return "/";
    case Instruction::UDiv:
        return "/";
    case Instruction::SRem:
        return "%";
    case Instruction::URem:
        return "%";
    case Instruction::And:
        return i1 ? "&&" : "&";
    case Instruction::Or:
        return i1 ? "||" : "|";
    default:
        return "?";
    }
}

static bool pairBin(Instruction::BinaryOps Op, Type *Ty, Instruction::BinaryOps &Out,
                    const char *&Kind) {
    switch (Op) {
    case Instruction::Add:
        Out = Instruction::Sub;
        Kind = "AOR";
        return true;
    case Instruction::Sub:
        Out = Instruction::Add;
        Kind = "AOR";
        return true;
    case Instruction::Mul:
        Out = Instruction::SDiv;
        Kind = "AOR";
        return true;
    case Instruction::SDiv:
        Out = Instruction::Mul;
        Kind = "AOR";
        return true;
    case Instruction::UDiv:
        Out = Instruction::Mul;
        Kind = "AOR";
        return true;
    case Instruction::SRem:
        Out = Instruction::SDiv;
        Kind = "AOR";
        return true;
    case Instruction::URem:
        Out = Instruction::UDiv;
        Kind = "AOR";
        return true;
    case Instruction::And:
        Out = Instruction::Or;
        Kind = (Ty && Ty->isIntegerTy(1)) ? "LCR" : "BOR";
        return true;
    case Instruction::Or:
        Out = Instruction::And;
        Kind = (Ty && Ty->isIntegerTy(1)) ? "LCR" : "BOR";
        return true;
    default:
        return false;
    }
}

static FunctionCallee getActiveFn(Module &M) {
    LLVMContext &Ctx = M.getContext();
    FunctionType *FT = FunctionType::get(Type::getInt1Ty(Ctx), {Type::getInt32Ty(Ctx)}, false);
    FunctionCallee C = M.getOrInsertFunction("mulation_active", FT);
    if (Function *F = dyn_cast<Function>(C.getCallee())) {
        F->setDoesNotThrow();
        F->addFnAttr(Attribute::NoInline);
        F->setDSOLocal(false);
    }
    return C;
}

static MutantRec makeRec(uint32_t id, const std::string &file, unsigned line, unsigned col,
                         const char *kind, const char *op, const char *mut) {
    MutantRec r;
    r.id = id;
    r.file = file;
    r.line = line;
    r.col = col;
    r.kind = kind;
    r.op = op;
    r.mut = mut;
    return r;
}

static unsigned instOrdinal(const Instruction *I) {
    unsigned n = 0;
    const Function *F = I->getFunction();
    if (!F) {
        return 0;
    }
    for (const BasicBlock &BB : *F) {
        for (const Instruction &II : BB) {
            if (&II == I) {
                return n;
            }
            ++n;
        }
    }
    return n;
}

static uint32_t idFor(const Instruction *I, const std::string &file, unsigned line, unsigned col,
                      const char *kind, const char *op, const char *mut) {
    std::string fn;
    if (const Function *F = I->getFunction()) {
        fn = F->getName().str();
    }
    std::string key = file + ":" + std::to_string(line) + ":" + std::to_string(col) + ":" + fn +
                      ":" + std::to_string(instOrdinal(I)) + ":" + kind + ":" + op + ":" + mut;
    return fnv1a(key);
}

static Instruction *insertAfterPoint(Instruction *I) {
    if (Instruction *N = I->getNextNode()) {
        return N;
    }
    return I;
}

class MulationInstrumentPass : public PassInfoMixin<MulationInstrumentPass> {
  public:
    static bool isRequired() { return true; }

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
        if (instrument(M)) {
            return PreservedAnalyses::none();
        }
        return PreservedAnalyses::all();
    }

  private:
    bool instrument(Module &M) {
        std::vector<BinaryOperator *> bins;
        std::vector<ICmpInst *> cmps;
        std::vector<std::pair<Instruction *, unsigned>> lits;

        for (Function &F : M) {
            if (F.isDeclaration() || F.isIntrinsic()) {
                continue;
            }
            if (F.getName() == "mulation_active") {
                continue;
            }
            for (BasicBlock &BB : F) {
                for (Instruction &I : BB) {
                    if (auto *BO = dyn_cast<BinaryOperator>(&I)) {
                        if (BO->getType()->isIntegerTy()) {
                            bins.push_back(BO);
                        }
                    } else if (auto *IC = dyn_cast<ICmpInst>(&I)) {
                        if (IC->getOperand(0)->getType()->isIntegerTy()) {
                            cmps.push_back(IC);
                        }
                    }
                    if (isa<ICmpInst>(I) || isa<ReturnInst>(I)) {
                        for (unsigned oi = 0, oe = I.getNumOperands(); oi < oe; ++oi) {
                            if (auto *C = dyn_cast<ConstantInt>(I.getOperand(oi))) {
                                if (C->getBitWidth() <= 64 && (C->isZero() || C->isOne()) &&
                                    !C->getType()->isIntegerTy(1)) {
                                    lits.push_back({&I, oi});
                                }
                            }
                        }
                    }
                }
            }
        }

        std::vector<MutantRec> recs;
        FunctionCallee Active = getActiveFn(M);
        bool changed = false;

        for (BinaryOperator *BO : bins) {
            Instruction::BinaryOps mutOp;
            const char *kind = nullptr;
            if (!pairBin(BO->getOpcode(), BO->getType(), mutOp, kind)) {
                continue;
            }
            std::string file;
            unsigned line = 0, col = 0;
            if (!locOf(BO, file, line, col)) {
                continue;
            }
            const char *opN = binName(BO->getOpcode(), BO->getType());
            const char *mutN = binName(mutOp, BO->getType());
            uint32_t id = idFor(BO, file, line, col, kind, opN, mutN);

            IRBuilder<> B(insertAfterPoint(BO));
            B.SetCurrentDebugLocation(BO->getDebugLoc());
            Value *MutV =
                B.CreateBinOp(mutOp, BO->getOperand(0), BO->getOperand(1), "mulation.mut");
            Value *Cond = B.CreateCall(Active, {B.getInt32(id)}, "mulation.on");
            Value *Sel = B.CreateSelect(Cond, MutV, BO, "mulation.sel");
            BO->replaceUsesWithIf(
                Sel, [&](Use &U) { return U.getUser() != MutV && U.getUser() != Sel; });
            recs.push_back(makeRec(id, file, line, col, kind, opN, mutN));
            changed = true;
        }

        for (ICmpInst *IC : cmps) {
            CmpInst::Predicate mutP;
            if (!pairPred(IC->getPredicate(), mutP)) {
                continue;
            }
            std::string file;
            unsigned line = 0, col = 0;
            if (!locOf(IC, file, line, col)) {
                continue;
            }
            const char *opN = predName(IC->getPredicate());
            const char *mutN = predName(mutP);
            uint32_t id = idFor(IC, file, line, col, "ROR", opN, mutN);

            IRBuilder<> B(insertAfterPoint(IC));
            B.SetCurrentDebugLocation(IC->getDebugLoc());
            Value *MutV = B.CreateICmp(mutP, IC->getOperand(0), IC->getOperand(1), "mulation.mut");
            Value *Cond = B.CreateCall(Active, {B.getInt32(id)}, "mulation.on");
            Value *Sel = B.CreateSelect(Cond, MutV, IC, "mulation.sel");
            IC->replaceUsesWithIf(
                Sel, [&](Use &U) { return U.getUser() != MutV && U.getUser() != Sel; });
            recs.push_back(makeRec(id, file, line, col, "ROR", opN, mutN));
            changed = true;
        }

        for (auto [I, oi] : lits) {
            auto *C = dyn_cast<ConstantInt>(I->getOperand(oi));
            if (!C) {
                continue;
            }
            std::string file;
            unsigned line = 0, col = 0;
            if (!locOf(I, file, line, col)) {
                continue;
            }
            const bool isZero = C->isZero();
            const char *opN = isZero ? "0" : "1";
            const char *mutN = isZero ? "1" : "0";
            uint32_t id = idFor(I, file, line, col, "LVR", opN, mutN);

            IRBuilder<> B(I);
            B.SetCurrentDebugLocation(I->getDebugLoc());
            Value *MutC =
                isZero ? ConstantInt::get(C->getType(), 1) : ConstantInt::get(C->getType(), 0);
            Value *Cond = B.CreateCall(Active, {B.getInt32(id)}, "mulation.on");
            Value *Sel = B.CreateSelect(Cond, MutC, C, "mulation.sel");
            I->setOperand(oi, Sel);
            recs.push_back(makeRec(id, file, line, col, "LVR", opN, mutN));
            changed = true;
        }

        if (recs.empty()) {
            return changed;
        }

        emitCatalog(M, recs);
        writeSidecar(M, recs);
        return true;
    }

    static std::string toNdjson(const std::vector<MutantRec> &recs) {
        std::string json;
        for (const MutantRec &r : recs) {
            json += "{\"id\":";
            json += std::to_string(r.id);
            json += ",\"file\":\"";
            json += jsonEscape(r.file);
            json += "\",\"line\":";
            json += std::to_string(r.line);
            json += ",\"col\":";
            json += std::to_string(r.col);
            json += ",\"kind\":\"";
            json += jsonEscape(r.kind);
            json += "\",\"op\":\"";
            json += jsonEscape(r.op);
            json += "\",\"mut\":\"";
            json += jsonEscape(r.mut);
            json += "\"}\n";
        }
        return json;
    }

    static void emitCatalog(Module &M, const std::vector<MutantRec> &recs) {
        std::string json = toNdjson(recs);
        LLVMContext &Ctx = M.getContext();
        Constant *Str = ConstantDataArray::getString(Ctx, json, true);
        auto *GV = new GlobalVariable(M, Str->getType(), true, GlobalValue::PrivateLinkage, Str,
                                      "mulation.catalog");
        GV->setSection("mulation_mutants");
        GV->setUnnamedAddr(GlobalValue::UnnamedAddr::None);
        GV->setAlignment(Align(1));

        appendToCompilerUsed(M, {GV});
    }

    static void writeSidecar(Module &M, const std::vector<MutantRec> &recs) {
        const char *dir = getenv("MULATION_CATALOG_DIR");
        if (!dir || dir[0] == '\0') {
            return;
        }
        std::error_code ec;
        sys::fs::create_directories(dir, true);
        uint32_t h = fnv1a(M.getModuleIdentifier());
        SmallString<256> path;
        sys::path::append(path, dir, std::to_string(h) + ".ndjson");
        raw_fd_ostream os(path, ec, sys::fs::OF_Text);
        if (ec) {
            return;
        }
        os << toNdjson(recs);
    }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "mulation", "0.1.0", [](PassBuilder &PB) {
                PB.registerPipelineStartEPCallback([](ModulePassManager &MPM, OptimizationLevel) {
                    MPM.addPass(MulationInstrumentPass());
                });
                PB.registerPipelineParsingCallback([](StringRef Name, ModulePassManager &MPM,
                                                      ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "mulation") {
                        MPM.addPass(MulationInstrumentPass());
                        return true;
                    }
                    return false;
                });
            }};
}
