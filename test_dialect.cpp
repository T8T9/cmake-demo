#include <iostream>

#include "llvm/ADT/Optional.h"
#include "mlir/Parser.h"
#include "mlir/IR/AsmState.h"
#include "mlir/IR/Module.h"
#include "mlir/IR/Block.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/IR/Verifier.h"
#include "llvm/Support/ScopedPrinter.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "mlir/Dialect/StandardOps/IR/Ops.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Transforms/Passes.h"

#include "dialect/test.h"
#include "dialect/types.h"

using namespace std;
namespace cl = llvm::cl;

static cl::opt<std::string> inputFilename(cl::Positional,
                                          cl::desc("<input toy file>"),
                                          cl::init("-"),
                                          cl::value_desc("filename"));

llvm::raw_ostream & printIndent(int indent=0) {
    for (int i = 0; i < indent; ++i)
        llvm::outs() << "    ";
    return llvm::outs();
}

void printOperation(mlir::Operation *op, int indent);
void printRegion(mlir::Region &region, int indent);
void printBlock(mlir::Block &block, int indent);

void printOperation(mlir::Operation *op, int indent) {
    // verify
    //if (failed(mlir::verify(op))) {
    //    op->emitError("verification error");
    //} else {
    //    printIndent() << "verify module \"" << op->getName() << "\" success\n";
    //}

    llvm::Optional<ModuleOp> module_op = llvm::None;
    if (llvm::isa<mlir::ModuleOp>(op)) module_op = llvm::dyn_cast<mlir::ModuleOp>(op);
    llvm::Optional<FuncOp> func_op = llvm::None;
    if (llvm::isa<mlir::FuncOp>(op)) func_op = llvm::dyn_cast<mlir::FuncOp>(op);

    printIndent(indent) << "op: '" << op->getName();
    // This getName is inherited from Operation::getName
    if (module_op) { printIndent() << "@" << module_op->getName(); }
    // This getName is inherited from SymbolOpInterfaceTrait::getName,
    // which return value of "sym_name" in ModuleOp or FuncOp attributes.
    if (func_op) { printIndent() << "@" << func_op->getName(); }
    printIndent() << "' with " << op->getNumOperands() << " operands"
        << ", " << op->getNumResults() << " results"
        << ", " << op->getAttrs().size() << " attributes"
        << ", " << op->getNumRegions() << " regions"
        << ", " << op->getNumSuccessors() << " successors\n";
    if (!op->getAttrs().empty()) {
        printIndent(indent) << op->getAttrs().size() << " attributes:\n";
        for (mlir::NamedAttribute attr : op->getAttrs()) {
            printIndent(indent + 1) << "- {" << attr.first << " : " << attr.second << "}\n";
        }
    }
    
    if (op->getNumRegions() > 0) {
        printIndent(indent) << op->getNumRegions() << " nested regions:\n";
        for (mlir::Region &region : op->getRegions()) {
            printRegion(region, indent + 1);
        }
    }
}

void printRegion(mlir::Region &region, int indent) {
    printIndent(indent) << "Region with " << region.getBlocks().size() << " blocks:\n";
    for (mlir::Block &block : region.getBlocks()) {
        printBlock(block, indent + 1);
    }
}

void printBlock(mlir::Block &block, int indent) {
    printIndent(indent) << "Block with " << block.getNumArguments() << " arguments"
        << ", " << block.getNumSuccessors() << " successors"
        << ", " << block.getOperations().size() << " operations\n";

    for (mlir::Operation &operation : block.getOperations()) {
        printOperation(&operation, indent + 1);
    }
}

    string mlir_source = R"src(
func @main() {
}
)src";

int main(int argc, char** argv) {
    mlir::registerAsmPrinterCLOptions();
    mlir::registerMLIRContextCLOptions();
    mlir::registerPassManagerCLOptions();
    cl::ParseCommandLineOptions(argc, argv, "mlir demo");

    mlir::MLIRContext *context = Global::getMLIRContext();
    context->allowUnregisteredDialects();
    context->getDialectRegistry().insert<test::TestDialect>();
    context->getDialectRegistry().insert<mlir::StandardOpsDialect>();

    // mlir will verify module automatically after parsing.
    // https://github.com/llvm/llvm-project/blob/38d18d93534d290d045bbbfa86337e70f1139dc2/mlir/lib/Parser/Parser.cpp#L2051
    //mlir::OwningModuleRef module_ref = mlir::parseSourceString(mlir_source, context);
    mlir::OwningModuleRef module_ref = mlir::parseSourceFile(inputFilename, context);
    std::cout << "----------print IR Structure begin----------" << std::endl;
    printOperation(module_ref->getOperation(), 0);
    std::cout << "----------print IR Structure end----------" << std::endl;

    mlir::Block *block = module_ref->getBody();
    // supress unused variable compile warning.
    (void)block;
    mlir::Region &region = module_ref->getBodyRegion();
    (void)region;

    for (auto iter = module_ref->begin(); iter != module_ref->end(); ++iter) {
        mlir::Operation &op = *iter;
        mlir::Dialect *dialect = op.getDialect();
        string dialect_name = dialect ? dialect->getNamespace().str() : "null";
        cout << op.getName().getStringRef().str() << ", namespace: " << dialect_name << endl;
        if (llvm::isa<mlir::FuncOp>(op)) {
            mlir::FuncOp func_op = llvm::dyn_cast<mlir::FuncOp>(op);
            cout << "func_op: " << func_op.getName().str() << endl;
        } else {
            cout << "non func_op:" << op.getName().getStringRef().str() << endl;
        }
    }

    module_ref->dump();

    // ==========test PassManager begin====================
    std::cout << "\n\n==========test PassManager begin==============" << std::endl;
    mlir::PassManager passManager(context);
    // Apply any generic pass manager command line options
    applyPassManagerCLOptions(passManager);
    // addPass 会调用 getCanonicalizationPatterns
    passManager.addNestedPass<mlir::FuncOp>(mlir::createCanonicalizerPass());
    // run pass pipeline
    if (mlir::failed(passManager.run(*module_ref))) {
        std::cerr << "PassManager run failed" << std::endl;
    } else {
        std::cout << "PassManager run succeeded." << std::endl;
    }
    module_ref->dump();
    std::cout << "==========test PassManager end==============" << std::endl;
    // ==========test PassManager end===================
    return 0;
}
